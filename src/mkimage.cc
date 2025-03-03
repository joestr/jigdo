/* $Id: mkimage.cc,v 1.15 2005/07/09 19:14:46 atterer Exp $ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2003  |  richard@
  | \/�|  Richard Atterer          |  atterer.org
  � '` �

  Copyright (C) 2016-2021 Steve McIntyre <steve@einval.com>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Create image from template / merge new files into image.tmp

*/

#include <config.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd-jigdo.h>

#include <iomanip>
#include <iostream>
#include <fstream>
#include <memory>

#include <compat.hh>
#include <log.hh>
#include <mkimage.hh>
#include <scan.hh>
#include <serialize.hh>
#include <string.hh>
#include <zstream-gz.hh>
#include <mktemplate.hh>

//______________________________________________________________________

DEBUG_UNIT("make-image")

namespace {

typedef JigdoDesc::ProgressReporter ProgressReporter;

// memset() is not portable enough...
void memClear(Ubyte* buf, size_t size) {
  while (size > 8) {
    *buf++ = 0; *buf++ = 0; *buf++ = 0; *buf++ = 0;
    *buf++ = 0; *buf++ = 0; *buf++ = 0; *buf++ = 0;
    size -= 8;
  }
  while (size > 0) {
    *buf++ = 0;
    --size;
  }
}

} // local namespace
//______________________________________________________________________

JigdoDesc::~JigdoDesc() { }
//______________________________________________________________________

bool JigdoDesc::isTemplate(bistream& file) {
  if (!file.seekg(0, ios::beg)) return false;
  string l;
  getline(file, l); // "JigsawDownload template 1.0 jigdo-file/0.0.1"
  string templHdr = TEMPLATE_HDR;
  if (compat_compare(l, 0, templHdr.length(), templHdr) != 0) return false;
  getline(file, l); // Ignore comment line
  getline(file, l); // Empty line, except for CR
  if (l != "\r") return false;
  return true;
}
//______________________________________________________________________

void JigdoDesc::seekFromEnd(bistream& file) {
  file.seekg(-6, ios::end);
  debug("JigdoDesc::seekFromEnd0: now at file offset %1",
        static_cast<uint64>(file.tellg()));
  uint64 descLen;
  SerialIstreamIterator f(file);
  unserialize6(descLen, f);
  if (static_cast<uint64>(file.tellg()) < descLen) { // Is this cast correct?
    debug("JigdoDesc::seekFromEnd1 descLen=%1", descLen);
    throw JigdoDescError(_("Invalid template data - corrupted file?"));
  }

  file.seekg(-descLen, ios::end);
  debug("JigdoDesc::seekFromEnd2: now at file offset %1",
        static_cast<uint64>(file.tellg()));

  size_t toRead = 4;
  Ubyte buf[4];
  buf[3] = '\0';
  Ubyte* b = buf;
  do {
    readBytes(file, b, toRead);
    size_t n = file.gcount();
    debug("JigdoDesc::seekFromEnd3: read %1, now at file offset %2",
          n, static_cast<uint64>(file.tellg()));
    //cerr<<"read "<<n<<' '<<file.tellg()<<endl;
    b += n;
    toRead -= n;
  } while (file.good() && toRead > 0);
  if (buf[0] != 'D' || buf[1] != 'E' || buf[2] != 'S' || buf[3] != 'C') {
    debug("JigdoDesc::seekFromEnd4 %1 %2 %3 %4",
          int(buf[0]), int(buf[1]), int(buf[2]), int(buf[3]));
    throw JigdoDescError(_("Invalid template data - corrupted file?"));
  }
}
//______________________________________________________________________

bistream& JigdoDescVec::get(bistream& file) {
  /* Need unique_ptr: If we did a direct push_back(new JigdoDesc), the
     "new" might succeed, but the push_back() fail with bad_alloc =>
     mem leak */
  unique_ptr<JigdoDesc> desc;
  clear();

  SerialIstreamIterator f(file);
  uint64 len;
  unserialize6(len, f); // descLen - 16, i.e. length of entries
  if (len < 45 || len > 256*1024*1024) {
    debug("JigdoDescVec::get: DESC section too small/large");
    throw JigdoDescError(_("Invalid template data - corrupted file?"));
  }
  len -= 16;
  //____________________

  uint64 off = 0; // Offset in image
  uint64 read = 0; // Nr of bytes read
  MD5 entryMd5;
  SHA256 entrySha256;
  uint64 entryLen;
  RsyncSum64 rsum;
  size_t blockLength;
  while (file && read < len) {
    Ubyte type = *f;
    ++f;
    switch (type) {

    case JigdoDesc::IMAGE_INFO_MD5:
      unserialize6(entryLen, f);
      unserialize(entryMd5, f);
      unserialize4(blockLength, f);
      if (!file) break;
      debug("JigdoDesc::read: ImageInfoMD5 %1 %2",
            entryLen, entryMd5.toString());
      desc.reset(new JigdoDesc::ImageInfoMD5(entryLen, entryMd5, blockLength));
      push_back(desc.release());
      read += 1 + 6 + entryMd5.serialSizeOf() + 4;
      break;

    case JigdoDesc::IMAGE_INFO_SHA256:
      unserialize6(entryLen, f);
      unserialize(entrySha256, f);
      unserialize4(blockLength, f);
      if (!file) break;
      debug("JigdoDesc::read: ImageInfoSHA256 %1 %2",
            entryLen, entrySha256.toString());
      desc.reset(new JigdoDesc::ImageInfoSHA256(entryLen, entrySha256, blockLength));
      push_back(desc.release());
      read += 1 + 6 + entrySha256.serialSizeOf() + 4;
      break;

    case JigdoDesc::UNMATCHED_DATA:
      unserialize6(entryLen, f);
      if (!file) break;
      debug("JigdoDesc::read: %1 UnmatchedData %2", off, entryLen);
      desc.reset(new JigdoDesc::UnmatchedData(off, entryLen));
      push_back(desc.release());
      read += 1 + 6;
      off += entryLen;
      break;

    case JigdoDesc::MATCHED_FILE_MD5:
    case JigdoDesc::WRITTEN_FILE_MD5:
      unserialize6(entryLen, f);
      unserialize(rsum, f);
      unserialize(entryMd5, f);
      if (!file) break;
      debug("JigdoDesc::read: %1 %2File %3 %4",
            off, (type == JigdoDesc::MATCHED_FILE_MD5 ? "Matched" : "Written"),
            entryLen, entryMd5.toString());
      if (type == JigdoDesc::MATCHED_FILE_MD5)
        desc.reset(new JigdoDesc::MatchedFileMD5(off, entryLen, rsum,entryMd5));
      else
        desc.reset(new JigdoDesc::WrittenFileMD5(off, entryLen, rsum,entryMd5));
      push_back(desc.release());
      read += 1 + 6 + rsum.serialSizeOf() + entryMd5.serialSizeOf();
      off += entryLen;
      break;

    case JigdoDesc::MATCHED_FILE_SHA256:
    case JigdoDesc::WRITTEN_FILE_SHA256:
      unserialize6(entryLen, f);
      unserialize(rsum, f);
      unserialize(entrySha256, f);
      if (!file) break;
      debug("JigdoDesc::read: %1 %2File %3 %4",
            off, (type == JigdoDesc::MATCHED_FILE_SHA256 ? "Matched" : "Written"),
            entryLen, entrySha256.toString());
      if (type == JigdoDesc::MATCHED_FILE_SHA256)
        desc.reset(new JigdoDesc::MatchedFileSHA256(off, entryLen, rsum,entrySha256));
      else
        desc.reset(new JigdoDesc::WrittenFileSHA256(off, entryLen, rsum,entrySha256));
      push_back(desc.release());
      read += 1 + 6 + rsum.serialSizeOf() + entrySha256.serialSizeOf();
      off += entryLen;
      break;

      // Template entry types that were obsoleted with version 0.6.3:

    case JigdoDesc::OBSOLETE_IMAGE_INFO:
      unserialize6(entryLen, f);
      unserialize(entryMd5, f);
      if (!file) break;
      debug("JigdoDesc::read: old ImageInfoMD5 %1 %2",
            entryLen, entryMd5.toString());
      // Special case: passing blockLength==0, which is otherwise impossible
      desc.reset(new JigdoDesc::ImageInfoMD5(entryLen, entryMd5, 0));
      push_back(desc.release());
      read += 1 + 6 + entryMd5.serialSizeOf();
      break;

    case JigdoDesc::OBSOLETE_MATCHED_FILE:
    case JigdoDesc::OBSOLETE_WRITTEN_FILE:
      unserialize6(entryLen, f);
      unserialize(entryMd5, f);
      if (!file) break;
      debug("JigdoDesc::read: %1 old %2File %3 %4", off,
            (type == JigdoDesc::OBSOLETE_MATCHED_FILE ? "Matched" :
             "Written"), entryLen, entryMd5.toString());
      /* Value of rsum is "don't care" because the ImageInfo's
         blockLength will be zero. */
      rsum.reset();
      if (type == JigdoDesc::OBSOLETE_MATCHED_FILE)
        desc.reset(new JigdoDesc::MatchedFileMD5(off, entryLen, rsum,entryMd5));
      else
        desc.reset(new JigdoDesc::WrittenFileMD5(off, entryLen, rsum,entryMd5));
      push_back(desc.release());
      read += 1 + 6 + entryMd5.serialSizeOf();
      off += entryLen;
      break;

    default:
      debug("JigdoDesc::read: unknown type %1", type);
      throw JigdoDescError(_("Invalid template data - corrupted file?"));
    }
  }
  //____________________

  if (read < len) {
    string err = subst(_("Error reading template data (%1)"),
                       strerror(errno));
    throw JigdoDescError(err);
  }

  if (empty())
    throw JigdoDescError(_("Invalid template data - corrupted file?"));

  // Sanity check for an imageInfoMD5/imageInfoSHA256 entry on the end of
  // the chain
  JigdoDesc::ImageInfoMD5* ii = dynamic_cast<JigdoDesc::ImageInfoMD5*>(back());
  JigdoDesc::ImageInfoSHA256* ii2 = dynamic_cast<JigdoDesc::ImageInfoSHA256*>(back());
  if ((ii == 0 && ii2 == 0)
      || (ii != 0 && ii->size() != off)
      || (ii2 != 0 && ii2->size() != off)) {
    throw JigdoDescError(_("Invalid template data - corrupted file?"));
  }
  return file;
}
//______________________________________________________________________

bostream& JigdoDescVec::put(bostream& file, MD5Sum* md, SHA256Sum* sd, int checksumChoice) const {
  // Pass 1: Accumulate sizes of entries, calculate descLen
  // 4 for DESC, 6 each for length of part at start & end
  uint64 descLen = 4 + 6*2; // Length of DESC part
  unsigned bufLen = 4 + 6;
  for (const_iterator i = begin(), e = end(); i != e; ++i) {
    unsigned s = (unsigned)(*i)->serialSizeOf();
    bufLen = max(bufLen, s);
    descLen += s;
  }
  if (DEBUG) bufLen += 1;

  // Pass 2: Write DESC part
  Ubyte buf[bufLen];
  if (DEBUG) buf[bufLen - 1] = 0xa5;
  Ubyte* p;
  p = serialize4(0x43534544, buf); // "DESC" in little-endian order
  p = serialize6(descLen, p);
  writeBytes(file, buf, 4 + 6);
  if (md != 0 && checksumChoice == MkTemplate::CHECK_MD5)
    md->update(buf, 4 + 6);
  if (sd != 0 && checksumChoice == MkTemplate::CHECK_SHA256)
    sd->update(buf, 4 + 6);
  for (const_iterator i = begin(), e = end(); i != e; ++i) {
    JigdoDesc::UnmatchedData* unm;
    JigdoDesc::ImageInfoMD5* infomd5;
    JigdoDesc::ImageInfoSHA256* infosha;
    JigdoDesc::MatchedFileMD5* matchedmd5;
    JigdoDesc::WrittenFileMD5* writtenmd5;
    JigdoDesc::MatchedFileSHA256* matchedsha;
    JigdoDesc::WrittenFileSHA256* writtensha;

    if ((unm = dynamic_cast<JigdoDesc::UnmatchedData*>(*i)) != 0)
      p = unm->serialize(buf);

    if ((infomd5 = dynamic_cast<JigdoDesc::ImageInfoMD5*>(*i)) != 0) {
      p = infomd5->serialize(buf);
      /* NB we must first try to cast to WrittenFile*, then to
         MatchedFile*, because WrittenFileMD5 derives from MatchedFile*. */
    } else if ((writtenmd5 = dynamic_cast<JigdoDesc::WrittenFileMD5*>(*i)) != 0) {
      p = writtenmd5->serialize(buf);
    } else if ((matchedmd5 = dynamic_cast<JigdoDesc::MatchedFileMD5*>(*i)) != 0) {
      p = matchedmd5->serialize(buf);
    } else if ((infosha = dynamic_cast<JigdoDesc::ImageInfoSHA256*>(*i)) != 0) {
      p = infosha->serialize(buf);
      /* NB we must first try to cast to WrittenFile*, then to
         MatchedFile*, because WrittenFileMD5 derives from MatchedFile*. */
    } else if ((writtensha = dynamic_cast<JigdoDesc::WrittenFileSHA256*>(*i)) != 0) {
      p = writtensha->serialize(buf);
    } else if ((matchedsha = dynamic_cast<JigdoDesc::MatchedFileSHA256*>(*i)) != 0) {
      p = matchedsha->serialize(buf);
    }

    writeBytes(file, buf, p - buf);
    if (md != 0 && checksumChoice == MkTemplate::CHECK_MD5)
      md->update(buf, p - buf);
    if (sd != 0 && checksumChoice == MkTemplate::CHECK_SHA256)
      sd->update(buf, p - buf);
  }
  p = serialize6(descLen, buf);
  writeBytes(file, buf, 6);
  if (md != 0)
    md->update(buf, 6);
  if (sd != 0)
    sd->update(buf, p - buf);
  if (DEBUG) { Assert(buf[bufLen - 1] == 0xa5); }
    return file;
}
//______________________________________________________________________

namespace {
  const int J_SIZE_WIDTH = 12;
  const int J_CSUM_WIDTH = 46;
  const int J_RSYNC_WIDTH = 12;
}

ostream& JigdoDesc::ImageInfoMD5::put(ostream& s) const {
  s << "image-info-md5    "
    << setw(J_SIZE_WIDTH) << size()
    << " " << setw(J_SIZE_WIDTH) << blockLength()
    << " " << setw(J_CSUM_WIDTH) << md5()
    << "\n";
  return s;
}
ostream& JigdoDesc::ImageInfoSHA256::put(ostream& s) const {
  s << "image-info-sha256 "
    << setw(J_SIZE_WIDTH) << size()
    << " " << setw(J_SIZE_WIDTH) << blockLength()
    << " " << setw(J_CSUM_WIDTH) << sha256()
    << "\n";
  return s;
}
ostream& JigdoDesc::UnmatchedData::put(ostream& s) const {
  s << "in-template       "
    << setw(J_SIZE_WIDTH) << offset()
    << " " << setw(J_SIZE_WIDTH) << size()
    << "\n";
  return s;
}
ostream& JigdoDesc::MatchedFileMD5::put(ostream& s) const {
  s << "need-file-md5     "
    << setw(J_SIZE_WIDTH) << offset()
    << " " << setw(J_SIZE_WIDTH) << size()
    << " " << setw(J_CSUM_WIDTH) << md5()
    << " " << setw(J_RSYNC_WIDTH) << rsync()
    << "\n";
  return s;
}
ostream& JigdoDesc::MatchedFileSHA256::put(ostream& s) const {
  s << "need-file-sha256  "
    << setw(J_SIZE_WIDTH) << offset()
    << " " << setw(J_SIZE_WIDTH) << size()
    << " " << setw(J_CSUM_WIDTH) << sha256()
    << " " << setw(J_RSYNC_WIDTH) << rsync()
    << "\n";
  return s;
}

ostream& JigdoDesc::WrittenFileMD5::put(ostream& s) const {
  s << "have-file        "
    << setw(J_SIZE_WIDTH) << offset()
    << " " << setw(J_SIZE_WIDTH) << size()
    << " " << setw(J_CSUM_WIDTH) << md5()
    << " " << setw(J_RSYNC_WIDTH) << rsync()
    << "\n";
  return s;
}
ostream& JigdoDesc::WrittenFileSHA256::put(ostream& s) const {
  s << "have-file-sha256 "
    << setw(J_SIZE_WIDTH) << offset()
    << " " << setw(J_SIZE_WIDTH) << size()
    << " " << setw(J_CSUM_WIDTH) << sha256()
    << " " << setw(J_RSYNC_WIDTH) << rsync()
    << "\n";
  return s;
}

void JigdoDescVec::list(ostream& s) throw() {
  for (const_iterator i = begin(), e = end(); i != e; ++i) s << (**i);
  s << flush;
}
//______________________________________________________________________

namespace {

  /* Helper functions for makeImage below, declared inline if only
     used once */

  /// Type of operation when recreating image data
  enum Task {
    CREATE_TMP, // Create a new .tmp file and copy some files into it,
                // maybe rename at end
    MERGE_TMP, // .tmp exists, copy over more files, maybe rename at end
    SINGLE_PASS // single-pass, all or nothing; writing to stdout
  };
  //______________________________

  inline void reportBytesWritten(const uint64 n, uint64& off,
      uint64& nextReport, const uint64 totalBytes,
      ProgressReporter& reporter) {
    off += n;
    if (off >= nextReport) { // Keep user entertained
      reporter.writingImage(off, totalBytes, off, totalBytes);
      nextReport += REPORT_INTERVAL;
    }
  }
  //______________________________

  /* Read up to file.size() of bytes from file, write it to image
     stream. Check MD5/rsync sum if requested. Take care not to write
     more than specified amount to image, even if file is longer. */
  int fileToImageMD5(bostream* img, FilePart& file,
      const JigdoDesc::MatchedFileMD5& matched, bool checkChecksum, size_t rsyncLen,
      ProgressReporter& reporter, Ubyte* buf, size_t readAmount, uint64& off,
      uint64& nextReport, const uint64 totalBytes) {
    uint64 toWrite = file.size();
    MD5Sum md;
    RsyncSum64 rs;
    size_t rl = 0; // Length covered by rs so far
    string fileName(file.getPath());
    fileName += file.leafName();
    bifstream f(fileName.c_str(), ios::binary);
    string err; // !err.empty() => error occurred

    // Read from file, write to image
    // First couple of k: Calculate RsyncSum rs and MD5Sum md
    if (checkChecksum && rsyncLen > 0) {
      while (*img && f && !f.eof() && toWrite > 0) {
        size_t n = (size_t)(toWrite < readAmount ? toWrite : readAmount);
        readBytes(f, buf, n);
        n = f.gcount();
        writeBytes(*img, buf, n);
        reportBytesWritten(n, off, nextReport, totalBytes, reporter);
        toWrite -= n;
        md.update(buf, n);
        // Update RsyncSum
        Paranoid(rl < rsyncLen);
        size_t rsyncToAdd = rsyncLen - rl;
        if (rsyncToAdd > n) rsyncToAdd = n;
        rs.addBack(buf, rsyncToAdd);
        rl += rsyncToAdd;
        Paranoid(rl <= rsyncLen);
        if (rl >= rsyncLen) break;
      }
    }
    // Rest of file: Only calculate MD5Sum md
    while (*img && f && !f.eof() && toWrite > 0) {
      size_t n = (size_t)(toWrite < readAmount ? toWrite : readAmount);
      readBytes(f, buf, n);
      n = f.gcount();
      writeBytes(*img, buf, n);
      reportBytesWritten(n, off, nextReport, totalBytes, reporter);
      toWrite -= n;
      if (checkChecksum) md.update(buf, n);
    }

    if (toWrite > 0 && (!f || f.eof())) {
      const char* errDetail = "";
      if (errno != 0) errDetail = strerror(errno);
      else if (f.eof()) errDetail = _("file is too short");
      err = subst(_("Error reading from `%1' (%2)"), fileName, errDetail);
      // Even if there was an error - always try to write right amount
      memClear(buf, readAmount);
      while (*img && toWrite > 0) {
        size_t n = (size_t)(toWrite < readAmount ? toWrite : readAmount);
        writeBytes(*img, buf, n);
        reportBytesWritten(n, off, nextReport, totalBytes, reporter);
        toWrite -= n;
      }
    } else if (checkChecksum
               && (md.finish() != matched.md5()
                   || (rsyncLen > 0 && rs != matched.rsync()))) {
      err = subst(_("Error: `%1' does not match checksum in template data"),
                  fileName);
    }

    if (err.empty()) return 0; // Success
    reporter.error(err);
    if (toWrite == 0)
      return 2; // "May have to fix something before you can continue"
    else
      return 3; // Yaargh, disaster! Please delete the .tmp file for me
  }
  //______________________________

  /* Read up to file.size() of bytes from file, write it to image
     stream. Check SHA256/rsync sum if requested. Take care not to
     write more than specified amount to image, even if file is
     longer. */
  int fileToImageSHA256(bostream* img, FilePart& file,
      const JigdoDesc::MatchedFileSHA256& matched, bool checkChecksum, size_t rsyncLen,
      ProgressReporter& reporter, Ubyte* buf, size_t readAmount, uint64& off,
      uint64& nextReport, const uint64 totalBytes) {
    uint64 toWrite = file.size();
    SHA256Sum md;
    RsyncSum64 rs;
    size_t rl = 0; // Length covered by rs so far
    string fileName(file.getPath());
    fileName += file.leafName();
    bifstream f(fileName.c_str(), ios::binary);
    string err; // !err.empty() => error occurred

    // Read from file, write to image
    // First couple of k: Calculate RsyncSum rs and SHA256Sum md
    if (checkChecksum && rsyncLen > 0) {
      while (*img && f && !f.eof() && toWrite > 0) {
        size_t n = (size_t)(toWrite < readAmount ? toWrite : readAmount);
        readBytes(f, buf, n);
        n = f.gcount();
        writeBytes(*img, buf, n);
        reportBytesWritten(n, off, nextReport, totalBytes, reporter);
        toWrite -= n;
        md.update(buf, n);
        // Update RsyncSum
        Paranoid(rl < rsyncLen);
        size_t rsyncToAdd = rsyncLen - rl;
        if (rsyncToAdd > n) rsyncToAdd = n;
        rs.addBack(buf, rsyncToAdd);
        rl += rsyncToAdd;
        Paranoid(rl <= rsyncLen);
        if (rl >= rsyncLen) break;
      }
    }
    // Rest of file: Only calculate SHA256Sum md
    while (*img && f && !f.eof() && toWrite > 0) {
      size_t n = (size_t)(toWrite < readAmount ? toWrite : readAmount);
      readBytes(f, buf, n);
      n = f.gcount();
      writeBytes(*img, buf, n);
      reportBytesWritten(n, off, nextReport, totalBytes, reporter);
      toWrite -= n;
      if (checkChecksum) md.update(buf, n);
    }

    if (toWrite > 0 && (!f || f.eof())) {
      const char* errDetail = "";
      if (errno != 0) errDetail = strerror(errno);
      else if (f.eof()) errDetail = _("file is too short");
      err = subst(_("Error reading from `%1' (%2)"), fileName, errDetail);
      // Even if there was an error - always try to write right amount
      memClear(buf, readAmount);
      while (*img && toWrite > 0) {
        size_t n = (size_t)(toWrite < readAmount ? toWrite : readAmount);
        writeBytes(*img, buf, n);
        reportBytesWritten(n, off, nextReport, totalBytes, reporter);
        toWrite -= n;
      }
    } else if (checkChecksum
               && (md.finish() != matched.sha256()
                   || (rsyncLen > 0 && rs != matched.rsync()))) {
      err = subst(_("Error: `%1' does not match checksum in template data"),
                  fileName);
    }

    if (err.empty()) return 0; // Success
    reporter.error(err);
    if (toWrite == 0)
      return 2; // "May have to fix something before you can continue"
    else
      return 3; // Yaargh, disaster! Please delete the .tmp file for me
  }
  //______________________________

  /* Write all bytes of the image data, i.e. both UnmatchedData and
     MatchedFile*s. If any UnmatchedFiles are present in 'files', write
     zeroes instead of the file content and also append a DESC section
     after the actual data.

     Why does this write zeroes, and not simply seek() forward the
     appropriate amount of bytes? - Because when seek() is used, a
     sparse file might be generated. This could result in "No room on
     device" later on - but we'd rather like that error as early as
     possible.

     @param name Filename corresponding to img
     @param totalBytes length of image

     if img==0, write to cout. If 0 is returned and not writing to
     cout, caller should rename file to remove .tmp extension. */
  inline int writeAll(const Task& task, JigdoDescVec& files,
      queue<FilePart*>& toCopy, bistream* templ, const size_t readAmount,
      bostream* img, const char* name, bool checkChecksum,
      ProgressReporter& reporter, JigdoCache* cache,
      const uint64 totalBytes) {

    bool isTemplate = JigdoDesc::isTemplate(*templ); // seek to 1st DATA part
    Assert(isTemplate);
    int result = 0;
    uint64 off = 0; // Current offset in image
    uint64 nextReport = 0; // At what value of off to call reporter
    uint64 blockLength = 0;
    uint64 imageSize = 0;

    vector<Ubyte> bufVec(readAmount);
    Ubyte* buf = &bufVec[0];
    /* Use an additional 8k of zip buffer. This is good if the
       unmatched image data is already compressed, which means that
       when it is compressed again by jigdo, it will get slightly
       larger. */
    unique_ptr<Zibstream> data(new Zibstream(*templ, (unsigned int)readAmount + 8*1024));
#   if HAVE_WORKING_FSTREAM
    if (img == 0) img = &cout; // EEEEEK!
#   else
    if (img == 0) img = &bcout;
#   endif

    /* First, find basic information about the image */
    JigdoDesc::ImageInfoMD5* imageInfoMD5 =
        dynamic_cast<JigdoDesc::ImageInfoMD5*>(files.back());
    if (imageInfoMD5 != 0) {
      blockLength = imageInfoMD5->blockLength();
      imageSize = imageInfoMD5->size();
    } else {
      JigdoDesc::ImageInfoSHA256* imageInfoSHA256 =
        dynamic_cast<JigdoDesc::ImageInfoSHA256*>(files.back());
      if (imageInfoSHA256) {
        blockLength = imageInfoSHA256->blockLength();
        imageSize = imageInfoSHA256->size();
      }
    }
    if (blockLength == 0 || imageSize == 0) {
      reporter.error(_("Unable to find a valid image info block"));
      return 3;
    }

    try {
      for (JigdoDescVec::iterator i = files.begin(), e = files.end();
           i != e; ++i) {
        //____________________

        /* Write all data for this part to img stream. In case of
           MatchedFile*, write the appropriate number of bytes (of junk
           data) even if file not present. [Using switch(type()) not
           nice, but using virtual methods looks even worse.] */
        switch ((*i)->type()) {
          case JigdoDesc::IMAGE_INFO_MD5:
          case JigdoDesc::IMAGE_INFO_SHA256:
            break;
          case JigdoDesc::UNMATCHED_DATA: {
            // Copy data from Zibstream to image.
            JigdoDesc::UnmatchedData& self =
                dynamic_cast<JigdoDesc::UnmatchedData&>(**i);
            uint64 toWrite = self.size();
            debug("mkimage writeAll(): %1 of unmatched data", toWrite);
            memClear(buf, readAmount);
            while (*img && toWrite > 0) {
              if (!*data) {
                reporter.error(_("Premature end of template data"));
                return 3;
              }
              data->read(buf, (unsigned int)(toWrite < readAmount ? toWrite : readAmount));
              size_t n = (size_t)data->gcount();
              writeBytes(*img, buf, n);
              reportBytesWritten(n, off, nextReport, totalBytes, reporter);
              toWrite -= n;
            }
            break;
          }
          case JigdoDesc::MATCHED_FILE_MD5: {
            /* If file present in cache, copy its data to image, if
               not, copy zeroes. if check==true, verify MD sum match.
               If successful, turn MatchedFileMD5 into WrittenFileMD5. */
            JigdoDesc::MatchedFileMD5* self =
                dynamic_cast<JigdoDesc::MatchedFileMD5*>(*i);
            uint64 toWrite = self->size();
            FilePart* mfile = 0;
            if (!toCopy.empty()) mfile = toCopy.front();
            debug("mkimage writeAll(): FilePart@%1, %2 of matched file `%3',"
                  " toCopy size %4", mfile, toWrite,
                  (mfile != 0 ? mfile->leafName() : ""), toCopy.size());
            if (mfile == 0 || self->md5() != *(mfile->getMD5Sum(cache))) {
              // Write right amount of zeroes
              memClear(buf, readAmount);
              while (*img && toWrite > 0) {
                size_t n = (size_t)(toWrite < readAmount ? toWrite : readAmount);
                writeBytes(*img, buf, n);
                reportBytesWritten(n, off, nextReport, totalBytes, reporter);
                toWrite -= n;
              }
              if (result == 0) result = 1; // Soft failure
            } else {
              /* Copy data from file to image, taking care not to
                 write beyond toWrite. */
              int status = fileToImageMD5(img, *mfile, *self, checkChecksum,
                  (size_t)blockLength, reporter, buf, readAmount, off,
                  nextReport, totalBytes);
              toCopy.pop();
              if (result < status) result = status;
              if (status == 0) { // Mark file as written to image
                *i = new JigdoDesc::WrittenFileMD5(self->offset(), self->size(),
                                                self->rsync(), self->md5());
                delete self;
              } else if (*img && (status > 2 || task == SINGLE_PASS)) {
                // If !*img, exit after error msg below
                /* If status <= 2 and task == {CREATE_TMP,MERGE_TMP},
                   we can continue; there has been an error copying
                   this individual file, but the right *amount* of
                   data has been written to the .tmp output file, and
                   the user may retry the failed one later. */
                return result;
              }
            }
            break;
          }
          case JigdoDesc::MATCHED_FILE_SHA256: {
            /* If file present in cache, copy its data to image, if
               not, copy zeroes. if check==true, verify SHA256 sum match.
               If successful, turn MatchedFileSHA256 into WrittenFileSHA256. */
            JigdoDesc::MatchedFileSHA256* self =
                dynamic_cast<JigdoDesc::MatchedFileSHA256*>(*i);
            uint64 toWrite = self->size();
            FilePart* mfile = 0;
            if (!toCopy.empty()) mfile = toCopy.front();
            debug("mkimage writeAll(): FilePart@%1, %2 of matched file `%3',"
                  " toCopy size %4", mfile, toWrite,
                  (mfile != 0 ? mfile->leafName() : ""), toCopy.size());
            if (mfile == 0 || self->sha256() != *(mfile->getSHA256Sum(cache))) {
              // Write right amount of zeroes
              memClear(buf, readAmount);
              while (*img && toWrite > 0) {
                size_t n = (size_t)(toWrite < readAmount ? toWrite : readAmount);
                writeBytes(*img, buf, n);
                reportBytesWritten(n, off, nextReport, totalBytes, reporter);
                toWrite -= n;
              }
              if (result == 0) result = 1; // Soft failure
            } else {
              /* Copy data from file to image, taking care not to
                 write beyond toWrite. */
              int status = fileToImageSHA256(img, *mfile, *self, checkChecksum,
                  (size_t)blockLength, reporter, buf, readAmount, off,
                  nextReport, totalBytes);
              toCopy.pop();
              if (result < status) result = status;
              if (status == 0) { // Mark file as written to image
                *i = new JigdoDesc::WrittenFileSHA256(self->offset(), self->size(),
						      self->rsync(), self->sha256());
                delete self;
              } else if (*img && (status > 2 || task == SINGLE_PASS)) {
                // If !*img, exit after error msg below
                /* If status <= 2 and task == {CREATE_TMP,MERGE_TMP},
                   we can continue; there has been an error copying
                   this individual file, but the right *amount* of
                   data has been written to the .tmp output file, and
                   the user may retry the failed one later. */
                return result;
              }
            }
            break;
          }
          case JigdoDesc::WRITTEN_FILE_MD5:
          case JigdoDesc::WRITTEN_FILE_SHA256:
          // These are never present in memory, cannot occur:
          case JigdoDesc::OBSOLETE_IMAGE_INFO:
          case JigdoDesc::OBSOLETE_MATCHED_FILE:
          case JigdoDesc::OBSOLETE_WRITTEN_FILE:
            debug("mkimage writeAll(): invalid type %1", (*i)->type());
            reporter.error(
                _("Error - template data's DESC section invalid"));
            Assert(false); // A WrittenFile* cannot occur here
            return 3;
            break;
        }
        //____________________

        // Error while writing to image?
        if (!*img) {
          string err = subst(_("Error while writing to `%1' (%2)"),
                             name, strerror(errno));
          reporter.error(err);
          return 3;
        }
        //____________________

      } // end iterating over 'files'

    } catch (Zerror e) {
      // Error while unpacking template data
      reporter.error(e.message); return 3;
    }

    // If we created a new tmp file, append DESC info
    if (task == CREATE_TMP && result > 0) {
      *img << files;
      if (!*img) return 3;
    }
    // Must have "used up" all the parts that we found earlier
    Assert(toCopy.empty());
    return result; // 0 or 1
  }
  //______________________________

  /* A temporary file already exists. Write the files listed in toCopy
     to this temporary file. If image is now completed, truncate it to
     its final length (removing the DESC section at the end),
     otherwise update the DESC section (turn some need-file/
     MatchedFile* entries into have-file/WrittenFile* entries). If 0 is
     returned, caller should rename file to remove .tmp extension. */
  inline int writeMerge(JigdoDescVec& files, queue<FilePart*>& toCopy,
      const int missing, const size_t readAmount, bfstream* img,
      const string& imageTmpFile, bool checkChecksum, ProgressReporter& reporter,
      JigdoCache* cache, const uint64 totalBytes) {
    vector<Ubyte> bufVec(readAmount);
    Ubyte* buf = &bufVec[0];
    int result = (missing == 0 ? 0 : 1);
    uint64 bytesWritten = 0; // For 'x% done' calls to reporter
    uint64 nextReport = 0; // At what value of bytesWritten to call reporter
    uint64 blockLength = 0;
    uint64 imageSize = 0;

    /* First, find basic information about the image */
    JigdoDesc::ImageInfoMD5* imageInfoMD5 =
        dynamic_cast<JigdoDesc::ImageInfoMD5*>(files.back());
    if (imageInfoMD5) {
      blockLength = imageInfoMD5->blockLength();
      imageSize = imageInfoMD5->size();
    } else {
      JigdoDesc::ImageInfoSHA256* imageInfoSHA256 =
        dynamic_cast<JigdoDesc::ImageInfoSHA256*>(files.back());
      if (imageInfoSHA256) {
        blockLength = imageInfoSHA256->blockLength();
        imageSize = imageInfoSHA256->size();
      }
    }
    if (blockLength == 0 || imageSize == 0) {
      reporter.error(_("Unable to find a valid image info block"));
      return 3;
    }

    if (toCopy.empty() && missing > 0) return 1;
    for (JigdoDescVec::iterator i = files.begin(), e = files.end();
         i != e; ++i) {

      JigdoDesc::MatchedFileMD5* matchMD5 =
        dynamic_cast<JigdoDesc::MatchedFileMD5*>(*i);
      JigdoDesc::MatchedFileSHA256* matchSHA256 =
        dynamic_cast<JigdoDesc::MatchedFileSHA256*>(*i);

      // Compare to 'case JigdoDesc::MATCHED_FILE_MD5:' clause in writeAll()
      if (matchMD5 != 0) {
        FilePart* mfile = 0;
	if (!toCopy.empty()) mfile = toCopy.front();
	debug("mkimage writeMerge(): FilePart@%1, %2 of matched file `%3', "
	      "toCopy size %4", mfile, matchMD5->size(),
	      (mfile != 0 ? mfile->leafName() : ""), toCopy.size());
	if (mfile == 0 || matchMD5->md5() != *(mfile->getMD5Sum(cache)))
          continue;

        /* Copy data from file to image, taking care not to write
           beyond matchMD5->size(). */
	img->seekp(matchMD5->offset(), ios::beg);
	if (!*img) {
          reporter.error(_("Error - could not access temporary file"));
	  result = 2;
	  break;
	}
	int status = fileToImageMD5(img, *mfile, *matchMD5, checkChecksum,
            (size_t)blockLength, reporter, buf, readAmount, bytesWritten,
            nextReport, totalBytes);
	toCopy.pop();
	if (result < status)
          result = status;
	if (status == 0) { // Mark file as written to image
          *i = new JigdoDesc::WrittenFileMD5(matchMD5->offset(),
					     matchMD5->size(),
					     matchMD5->rsync(),
					     matchMD5->md5());
	  delete matchMD5;
	} else if (status > 2) {
          break;
	}
      } else if (matchSHA256 != 0) {
        FilePart* mfile = 0;
	if (!toCopy.empty()) mfile = toCopy.front();
	debug("mkimage writeMerge(): FilePart@%1, %2 of matched file `%3', "
	      "toCopy size %4", mfile, matchSHA256->size(),
	      (mfile != 0 ? mfile->leafName() : ""), toCopy.size());
	if (mfile == 0 || matchSHA256->sha256() != *(mfile->getSHA256Sum(cache)))
          continue;

        /* Copy data from file to image, taking care not to write
           beyond matchSHA256->size(). */
	img->seekp(matchSHA256->offset(), ios::beg);
	if (!*img) {
          reporter.error(_("Error - could not access temporary file"));
	  result = 2;
	  break;
	}
	int status = fileToImageSHA256(img, *mfile, *matchSHA256, checkChecksum,
            (size_t)blockLength, reporter, buf, readAmount, bytesWritten,
            nextReport, totalBytes);
	toCopy.pop();
	if (result < status)
          result = status;
	if (status == 0) { // Mark file as written to image
          *i = new JigdoDesc::WrittenFileSHA256(matchSHA256->offset(),
						matchSHA256->size(),
						matchSHA256->rsync(),
						matchSHA256->sha256());
	  delete matchSHA256;
	} else if (status > 2) {
          break;
	}
      } else
        /* no match for either matchMD5 or matchSHA256 */
        continue;
    } // end iterating over 'files'

    if (missing == 0 && result == 0) {
      img->close(); // Necessary on Windows before truncating is possible
      // Truncate to final image size
      const char* tmpName = imageTmpFile.c_str();
      if (compat_truncate(tmpName, imageSize) != 0) {
        string err = subst(_("Could not truncate `%1' (%2)"),
                           imageTmpFile, strerror(errno));
        reporter.error(err);
        return 3;
      }
      return 0;
    } else {
      // Update DESC section at end of temporary file
      img->seekp(imageSize);
      // No need to truncate here because DESC section never changes size
      *img << files;
      if (!*img) return 3;
      return result;
    }
  }
  //______________________________

  int info_NeedMoreFiles(ProgressReporter& reporter, const string& tmpName) {
    string info = subst(_(
          "Copied input files to temporary file `%1' - "
          "repeat command and supply more files to continue"), tmpName);
    reporter.info(info);
    return 1; // Soft failure
  }

  int error_CouldntRename(ProgressReporter& reporter, const char* name,
                          const char* finalName) {
    string err = subst(_(
        "Could not move finished image from `%1' to `%2' (%3)"),
        name, finalName, strerror(errno));
    reporter.error(err);
    return 3;
  }

} // end local namespace
//______________________________________________________________________

namespace {

  /// Read template data from templ (name in templFile) into files
  void readTemplate(JigdoDescVec& files, const string& templFile,
                    bistream* templ) {
    if (JigdoDesc::isTemplate(*templ) == false) { // Check for template hdr
      string err = subst(_("`%1' is not a template file"), templFile);
      throw JigdoDescError(err);
    }
    /* Read info at end of template data. NB: Exceptions are not
       caught here, but e.g. in ::makeImage() (cf. jigdo-file.cc) */
    JigdoDesc::seekFromEnd(*templ);
    *templ >> files;
  }
  //________________________________________

  /** Read data from end of temporary file imageTmp, output it to
      filesTmp. Next, compare it to template data in "files". If tmp
      file is OK for re-using return NULL - this means that the DESC
      entries match *exactly* - the only difference allowed is
      MatchedFile* turning into WrittenFile*. Otherwise, return a
      pointer to an error message describing the reason why the
      tmpfile data does not match the template data. */
  const char* readTmpFile(bistream& imageTmp, JigdoDescVec& filesTmp,
                          const JigdoDescVec& files) {
    try {
      JigdoDesc::seekFromEnd(imageTmp);
      imageTmp >> filesTmp;
    } catch (JigdoDescError e) {
      return _("it was not created by jigdo-file, or is corrupted.");
    }
    if (*files.back() != *filesTmp.back())
      return _("it corresponds to a different image/template.");
    if (files.size() != filesTmp.size())
      return _("since its creation, the template was regenerated.");
    for (size_t i = 0; i < files.size() - 1; ++i) {
      //cerr << "cmp " << i << '/' << (files.size() - 1) << endl;
      if (*files[i] != *filesTmp[i])
        return _("since its creation, the template was regenerated.");
    }
    return 0;
  }

}
//________________________________________

/* If imageTmpFile.empty(), must either write whole image or nothing
   at all. image and temporary file are created as needed, ditto for
   renaming of temporary to image. The cache must not throw errors. */
int JigdoDesc::makeImage(JigdoCache* cache, const string& imageFile,
    const string& imageTmpFile, const string& templFile,
    bistream* templ, const bool optForce, ProgressReporter& reporter,
    const size_t readAmount, const bool optMkImageCheck) {

  Task task = CREATE_TMP;

  if (imageFile == "-" || imageTmpFile.empty()) task = SINGLE_PASS;
  //____________________

  // Read info from template
  JigdoDescVec files;
  readTemplate(files, templFile, templ);
  //____________________

  // Do we need to add new stuff to an existing tmp file?
  bfstream* img = 0; // Non-null => tmp file exists
  unique_ptr<bfstream> imgDel(img);
  struct stat fileInfo;
  if (task != SINGLE_PASS && stat(imageTmpFile.c_str(), &fileInfo) == 0) {
    /* A tmp file already exists. We'll only reuse it if the DESC
       entries match exactly. Otherwise, if --force enabled, overwrite
       it, else error. */
    const char* wontReuse = 0; // non-NULL means: will not reuse tmp file
    JigdoDescVec filesTmp;
    imgDel.reset(new bfstream(imageTmpFile.c_str(),
                              ios::binary|ios::in|ios::out));
    img = imgDel.get();
    if (!*img)
      wontReuse = strerror(errno);
    else
      wontReuse = readTmpFile(*img, filesTmp, files);

    if (wontReuse != 0) {
      // Print out message
      string msg = subst(_("Will not reuse existing temporary file `%1' - "
                           "%2"), imageTmpFile, wontReuse);
      // Return error if not allowed to overwrite tmp file
      if (!optForce) {
        reporter.error(msg);
        throw Error(_("Delete/rename the file or use --force"));
      }
      // Open a new tmp file later (imgDel will close() this one for us)
      reporter.info(msg);
      img = 0;
      Paranoid(task == CREATE_TMP);
    } else {
      // Reuse temporary file
      task = MERGE_TMP;
      files.swap(filesTmp);
      Assert(!filesTmp.empty() && img != 0);
    }
  } // endif (tmp file exists)

  Paranoid((task == MERGE_TMP) == (img != 0));
  //____________________

  /* Variables now in use:
     enum task:
             Mode of operation (CREATE_TMP/MERGE_TMP/SINGLE_PASS)
     JigdoDescVec files:
             Contents of image, maybe with some WrittenFile*s if MERGEing
     istream* templ: Template data (stream pointer at end of file)
     fstream* img: Temporary file if MERGE_TMP, else null
  */

  /* Create queue of files that need to be copied to the image. Later
     on, we will be pop()ing to get to the actual filenames in order.
     Referenced FileParts are owned by the JigdoCache - never delete
     them. */
  queue<FilePart*> toCopy;
  int missing = 0; // Nr of files that were not found
  JigdoCache::iterator ci, ce = cache->end();
  uint64 totalBytes = 0; // Total amount of data to be written, for "x% done"

  for (vector<JigdoDesc*>::iterator i = files.begin(), e = files.end();
       i != e; ++i) {
    // Need this extra test because we do *not* want the WrittenFile*s
    switch ((*i)->type()) {

      case MATCHED_FILE_MD5:
      {
        MatchedFileMD5* m = dynamic_cast<MatchedFileMD5*>(*i);
        Paranoid(m != 0);
        //totalBytes += m->size();

        // Search for file with matching MD5 sum
	ci = cache->begin();
	bool found = false;
	while (ci != ce) {
          // The call to getMD5Sum() may cause the whole file to be read!
          const MD5Sum* md = ci->getMD5Sum(cache);
	  if (md != 0 && *md == m->md5()) {
            toCopy.push(&*ci); // Found matching file
	    totalBytes += m->size();
	    debug("%1 found, pushed %2", m->md5().toString(), &*ci);
	    found = true;
	    break;
	  }
	  ++ci;
	}
	if (!found)
          ++missing;
      }
      break;

      case MATCHED_FILE_SHA256:
      {
        MatchedFileSHA256* m = dynamic_cast<MatchedFileSHA256*>(*i);
        Paranoid(m != 0);
        //totalBytes += m->size();

        // Search for file with matching SHA256 sum
	ci = cache->begin();
	bool found = false;
	while (ci != ce) {
          // The call to getSHA256Sum() may cause the whole file to be read!
          const SHA256Sum* md = ci->getSHA256Sum(cache);
	  if (md != 0 && *md == m->sha256()) {
            toCopy.push(&*ci); // Found matching file
	    totalBytes += m->size();
	    debug("%1 found, pushed %2", m->sha256().toString(), &*ci);
	    found = true;
	    break;
	  }
	  ++ci;
	}
	if (!found)
          ++missing;
      }
      break;

      default:
        continue;
	break;

    }
  }
  //____________________

  debug("JigdoDesc::mkImage: %1 missing, %2 found for copying to image, "
        "%3 entries in template", missing, toCopy.size(), files.size());

  // Files appearing >1 times are counted >1 times for the message
  string missingInfo = subst(
      _("Found %1 of the %2 files required by the template"),
      toCopy.size(), toCopy.size() + missing);
  reporter.info(missingInfo);
  //____________________

  /* There used to be the following here:
     | If possible (i.e. all files present, tmp file not yet created),
     | avoid creating any tmp file at all.
     | if (task == CREATE_TMP && missing == 0) task = SINGLE_PASS;
     We do not do this because even though it says "missing==0" *now*,
     there could be a read error from one of the files when we
     actually access it, in which case we should be able to ignore the
     error for the moment, and leave behind a partially complete .tmp
     file. */

  /* Do nothing at all if a) no tmp file created yet, and b) *none* of
     the supplied files matched one of the missing parts, and c) the
     template actually contains at least one MatchedFile* (i.e. *do*
     write if template consists entirely of UnmatchedData). */
# ifndef MKIMAGE_ALWAYS_CREATE_TMPFILE
  if (task == CREATE_TMP && toCopy.size() == 0 && missing != 0) {
    const char* m = _("Will not create image or temporary file - try again "
                      "with different input files");
    reporter.info(m);
    return 1; // Return value: "Soft failure - may retry with more files"
  }

  // Give error if unable to create image in one pass
  if (task == SINGLE_PASS && missing > 0) {
    reporter.error(_("Cannot create image because of missing files"));
    return 3; // Permanent failure
  }
# endif
  //____________________

  if (task == MERGE_TMP) { // If MERGEing, img was already set up above
    int result = writeMerge(files, toCopy, missing, readAmount, img,
                            imageTmpFile, optMkImageCheck, reporter, cache,
                            totalBytes);
    if (missing != 0 && result < 3)
      info_NeedMoreFiles(reporter, imageTmpFile);
    if (result == 0) {
      if (compat_rename(imageTmpFile.c_str(), imageFile.c_str()) != 0)
        return error_CouldntRename(reporter, imageTmpFile.c_str(),
                                   imageFile.c_str());
      string info = subst(_("Successfully created `%1'"), imageFile);
      reporter.info(info);
    }
    return result;
  }

  // task == CREATE_TMP || task == SINGLE_PASS

  // Assign a stream to img which we're going to write image data to
  // If necessary, create a new temporary/output file
  const char* name;
  const char* finalName = 0;
  if (task == CREATE_TMP) { // CREATE new tmp file
    name = imageTmpFile.c_str();
    finalName = imageFile.c_str();
    imgDel.reset(new bfstream(name, ios::binary|ios::out|ios::trunc));
    img = imgDel.get();
  } else if (imageFile != "-") { // SINGLE_PASS; create output file
    name = imageFile.c_str();
    imgDel.reset(new bfstream(name, ios::binary|ios::out|ios::trunc));
    img = imgDel.get();
  } else { // SINGLE_PASS, outputting to stdout
    name = "-";
    imgDel.reset(0);
    img = 0; // Cannot do "img = &cout", so img==0 is special case: stdout
  }
  if (img != 0 && !*img) {
    string err = subst(_("Could not open `%1' for output: %2"),
                       name, strerror(errno));
    reporter.error(err);
    return 3; // Permanent failure
  }

  /* Above, totalBytes was calculated for the case of a MERGE_TMP. If
     we're not merging, we need to write everything. */
  Assert(files.back()->type() == IMAGE_INFO_MD5 ||
	 files.back()->type() == IMAGE_INFO_SHA256);
  uint64 imageSize = files.back()->size();
  totalBytes = imageSize;
# if 0 /* # if WINDOWS */
  /* The C++ STL of the MinGW 1.1 gcc port for Windows doesn't support
     files >2GB. Fail early and with a clear error message... */
  if (imageSize >= (1U<<31))
    throw Error(_("Sorry, at the moment the Windows port of jigdo cannot "
                  "create files bigger than 2 GB. Use the Linux version."));
# endif

  int result = writeAll(task, files, toCopy, templ, readAmount, img, name,
                        optMkImageCheck, reporter, cache, totalBytes);
  if (result >= 3) return result;

  if (task == CREATE_TMP && result == 1) {
    info_NeedMoreFiles(reporter, imageTmpFile);
  } else if (result == 0) {
    if (img != 0)
      img->close(); // Necessary on Windows before renaming is possible
    if (finalName != 0 && compat_rename(name, finalName) != 0)
      return error_CouldntRename(reporter, name, finalName);
    string info = subst(_("Successfully created `%1'"), imageFile);
    reporter.info(info);
  }
  return result;
}
//______________________________________________________________________

int JigdoDesc::listMissingMD5(set<MD5>& result, const string& imageTmpFile,
    const string& templFile, bistream* templ, ProgressReporter& reporter) {
  result.clear();

  // Read info from template
  JigdoDescVec contents;
  readTemplate(contents, templFile, templ);

  // Read info from temporary file, if any
  if (!imageTmpFile.empty()) {
    bifstream imageTmp(imageTmpFile.c_str(), ios::binary);
    if (imageTmp) {
      JigdoDescVec contentsTmp;
      const char* wontReuse = readTmpFile(imageTmp, contentsTmp, contents);
      if (wontReuse != 0) {
        string msg = subst(_("Ignoring existing temporary file `%1' - %2"),
                           imageTmpFile, wontReuse);
        reporter.info(msg);
      } else {
        // tmp file present & valid - use *it* below to output missing parts
        swap(contents, contentsTmp);
      }
    }
  }

  // Output MD5 sums of MatchedFileMD5 (but not WrittenFileMD5) entries
  for (size_t i = 0; i < contents.size() - 1; ++i) {
    MatchedFileMD5* mf = dynamic_cast<MatchedFileMD5*>(contents[i]);
    if (mf != 0 && mf->type() == MATCHED_FILE_MD5)
      result.insert(mf->md5());
  }
  return 0;
}
//______________________________________________________________________

int JigdoDesc::listMissingSHA256(set<SHA256>& result, const string& imageTmpFile,
    const string& templFile, bistream* templ, ProgressReporter& reporter) {
  result.clear();

  // Read info from template
  JigdoDescVec contents;
  readTemplate(contents, templFile, templ);

  // Read info from temporary file, if any
  if (!imageTmpFile.empty()) {
    bifstream imageTmp(imageTmpFile.c_str(), ios::binary);
    if (imageTmp) {
      JigdoDescVec contentsTmp;
      const char* wontReuse = readTmpFile(imageTmp, contentsTmp, contents);
      if (wontReuse != 0) {
        string msg = subst(_("Ignoring existing temporary file `%1' - %2"),
                           imageTmpFile, wontReuse);
        reporter.info(msg);
      } else {
        // tmp file present & valid - use *it* below to output missing parts
        swap(contents, contentsTmp);
      }
    }
  }

  // Output SHA256 sums of MatchedFileSHA256 (but not WrittenFileSHA256) entries
  for (size_t i = 0; i < contents.size() - 1; ++i) {
    MatchedFileSHA256* mf = dynamic_cast<MatchedFileSHA256*>(contents[i]);
    if (mf != 0 && mf->type() == MATCHED_FILE_SHA256)
      result.insert(mf->sha256());
  }
  return 0;
}
//______________________________________________________________________

void JigdoDesc::ProgressReporter::error(const string& message) {
  cerr << message << endl;
}
void JigdoDesc::ProgressReporter::info(const string& message) {
  cerr << message << endl;
}
void JigdoDesc::ProgressReporter::writingImage(uint64, uint64, uint64,
                                              uint64) { }

JigdoDesc::ProgressReporter JigdoDesc::noReport;
