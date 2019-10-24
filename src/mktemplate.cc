/* $Id: mktemplate.cc,v 1.63 2002/04/10 20:07:41 richard Exp $ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2002 Richard Atterer
  | \/¯|  <richard@atterer.net>
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Create location list (.jigdo) and image template (.template)

  WARNING: This is very complicated code - be sure to run the
  "torture" regression test after making changes. Read this file from
  bottom to top.

*/

#include <errno.h>
#include <math.h>
#include <string.h>
#include <zlib.h>

#include <algorithm>
#include <iostream>
#include <fstream>
#include <map>
#include <memory>
namespace std { }
using namespace std;

#include <config.h>
#include <configfile.hh>
#include <compat.hh>
#include <autoptr.hh>
#include <debug.hh>
#include <mkimage.hh>
#include <mktemplate.hh>
#include <scan.hh>
#include <string.hh>
#include <zstream.hh>

/* In the log messages that are output, uppercase means that it is
   finally decided what this area of the image is: UNMATCHED or a
   MATCH of a file. */
#ifndef DEBUG_MKTEMPLATE
#  define DEBUG_MKTEMPLATE (DEBUG && 0)
#endif
//______________________________________________________________________

void MkTemplate::ProgressReporter::error(const string& message) {
  cerr << message << endl;
}
void MkTemplate::ProgressReporter::scanningImage(uint64) { }
void MkTemplate::ProgressReporter::matchFound(const FilePart*, uint64) { }
void MkTemplate::ProgressReporter::finished(uint64) { }

MkTemplate::ProgressReporter MkTemplate::noReport;
//______________________________________________________________________

MkTemplate::MkTemplate(JigdoCache* jcache, bistream* imageStream,
    ostream* jigdoStream, bostream* templateStream, ProgressReporter& pr,
    int zipQuality, size_t readAmnt, bool addImage, bool addServers)
  : fileSizeTotal(0U), fileCount(0U), block(), readAmount(readAmnt),
    cache(jcache),
    image(imageStream), jigdo(jigdoStream), templ(templateStream),
    zipQual(zipQuality), reporter(pr),
    addImageSection(addImage),
    addServersSection(addServers) {
}
//______________________________________________________________________

namespace {

  // Find the position of the highest set bit (e.g. for 0x20, result is 5)
  inline int bitWidth(uint32 x) {
    int r = 0;
    Assert(x <= 0xffffffff); // Can't cope with >32 bits
    if (x & 0xffff0000) { r += 16; x >>= 16; }
    if (x & 0xff00) { r += 8; x >>= 8; }
    if (x & 0xf0) { r += 4; x >>= 4; }
    if (x & 0xc) { r += 2; x >>= 2; }
    if (x & 2) { r += 1; x >>=1; }
    if (x & 1) r += 1;
    return r;
  }
  //________________________________________

  /* Avoid integer divisions: Modulo addition/subtraction, with
     certain assertions */
  // Returns (a + b) % m, if (a < m && b <= m)
  inline size_t modAdd(size_t a, size_t b, size_t m) {
    size_t r = a + b;
    if (r >= m) r -= m;
    Paranoid(r == (a + b) % m);
    return r;
  }
  // Returns (a - b) mod m, if (a < m && b <= m)
  inline size_t modSub(size_t a, size_t b, size_t m) {
    size_t r = a + m - b;
    if (r >= m) r -= m;
    Paranoid(r == (m + a - b) % m);
    return r;
  }
  // Returns (a + b) % m, if (a < m && -m <= b && b <= m)
  inline size_t modAdd(size_t a, int b, size_t m) {
    size_t r = a;
    if (b < 0) r += m;
    r += b;
    if (r >= m) r -= m;
    Paranoid(r == (a + static_cast<size_t>(b + m)) % m);
    return r;
  }
  //____________________

  /* Write part of a circular buffer to a Zobstream object, starting
     with offset "start" (incl) and ending with offset "end" (excl).
     Offsets can be equal to bufferLength. If both offsets are equal,
     the whole buffer content is written. */
  inline void writeBuf(const byte* const buf, size_t begin, size_t end,
                       const size_t bufferLength, Zobstream& zip) {
    Paranoid(begin <= bufferLength && end <= bufferLength);
    if (begin < end) {
      zip.write(buf + begin, end - begin);
    } else {
      zip.write(buf + begin, bufferLength - begin);
      zip.write(buf, end);
    }
  }
  //____________________

  // Write lower 48 bits of x to s in little-endian order
  void write48(bostream& s, uint64 x) {
    s << static_cast<byte>(x & 0xff)
      << static_cast<byte>((x >> 8) & 0xff)
      << static_cast<byte>((x >> 16) & 0xff)
      << static_cast<byte>((x >> 24) & 0xff)
      << static_cast<byte>((x >> 32) & 0xff)
      << static_cast<byte>((x >> 40) & 0xff);
  }

} // namespace
//______________________________________________________________________

/* Build up a template DESC section by appending data items to a
   string<byte>. Calls to descUnmatchedData() are allowed to
   accumulate, so that >1 consecutive unmatched data areas are merged
   into one in the DESC section. */
class MkTemplate::Desc {
public:
  Desc() : files(), offset(0) { }

  // Insert in DESC section: information about whole image
  inline void imageInfo(uint64 len, const MD5Sum& md5, size_t blockLength) {
    files.reserve((files.size() + 16) % 16);
    files.push_back(new JigdoDesc::ImageInfo(len, md5, blockLength));
  }
  // Insert in DESC section: info about some data that was not matched
  inline void unmatchedData(uint64 len) {
    JigdoDesc::UnmatchedData* u;
    if (files.size() > 0
        && (u = dynamic_cast<JigdoDesc::UnmatchedData*>(files.back())) !=0) {
      // Add more unmatched data to already existing UnmatchedData object
      u->resize(u->size() + len);
    } else {
      // Create new UnmatchedData object.
      files.reserve((files.size() + 16) % 16);
      files.push_back(new JigdoDesc::UnmatchedData(offset, len));
    }
    offset += len;
  }
  // Insert in DESC section: information about a file that matched
  inline void matchedFile(uint64 len, const RsyncSum64& r,
                          const MD5Sum& md5) {
    files.reserve((files.size() + 16) % 16);
    files.push_back(new JigdoDesc::MatchedFile(offset, len, r, md5));
    offset += len;
  }
  inline bostream& put(bostream& s) { s << files; return s; }
private:
  JigdoDescVec files;
  uint64 offset;
};
//______________________________________________________________________

/* The following are helpers used by run(). It is usually not adequate
   to make such large functions inline, but there is only one call to
   them, anyway. */

inline bool MkTemplate::scanFiles(size_t blockLength, uint32 blockMask,
                                  size_t md5BlockLength) {
  bool result = SUCCESS;

  cache->setParams(blockLength, md5BlockLength);
  FileVec::iterator hashPos;

  for (JigdoCache::iterator file = cache->begin();
       file != cache->end(); ++file) {
    const RsyncSum64* sum = file->getRsyncSum(cache);
    if (sum == 0) continue; // Error - skip
    // Add file to hash list
    hashPos = block.begin() + (sum->getHi() & blockMask);
    hashPos->push_back(&*file);
  }
  return result;
}
//________________________________________

// One object for each offset in image where any file /might/ match
struct MkTemplate::PartialMatch {
  uint64 startOff; // Offset in image at which this match starts
  uint64 nextEvent; // Next value of off at which to finish() sum & compare
  size_t blockOff; // Offset in buf of start of current MD5 block
  size_t blockNr; // Number of block in file, i.e. index into file->sums[]
  FilePart* file; // File whose sums matched so far
  PartialMatch* next;
  bool operator<=(const PartialMatch& x) {
    return nextEvent <= x.nextEvent;
  }
};
//________________________________________

/* Insert x in todo using linear search, possibly updating todo. The
   elements of todo are always sorted by ascending nextEvent. */
void MkTemplate::insertInTodo(PartialMatch*& todo, PartialMatch* x) {
  if (todo == 0 || *x <= *todo) {
    x->next = todo;
    todo = x;
    return;
  }
  PartialMatch* cur = todo;
  while (true) {
    if (cur->next == 0) {
      x->next = 0;
      cur->next = x;
      return;
    } else if (*x <= *cur->next) {
      x->next = cur->next;
      cur->next = x;
      return;
    }
    cur = cur->next;
  }
}
//________________________________________

void MkTemplate::checkRsyncSumMatch2(const size_t blockLen,
    const size_t back, const uint64 off, const size_t md5BlockLength,
    PartialMatch*& todo, PartialMatch*& todoFree, uint64& nextEvent,
    const uint64 unmatchedStart, FilePart* file) {
  /* Rolling rsum matched - schedule an MD5Sum match. NB: In extreme
     cases, nextEvent may be equal to off */
  PartialMatch* x = todoFree;
  x->startOff = off - blockLen;
  /* Don't schedule match if its startOff is impossible. This is e.g.
     the case when file A is 4096 bytes long (i.e. is immediately
     matched once seen), and the first 4096 bytes of file B are equal
     to file A, and file B is in the image. */
  if (x->startOff < unmatchedStart) return;
  todoFree = todoFree->next;
  x->nextEvent = x->startOff +
    (file->size() < md5BlockLength ? file->size() : md5BlockLength);
# if DEBUG_MKTEMPLATE
  cerr << ' ' << off << ": Head of " << file->leafName()
       << " match at offset " << x->startOff << ", my next event "
       << x->nextEvent << endl;
# endif
  if (x->nextEvent < nextEvent) nextEvent = x->nextEvent;
  x->blockOff = back;
  x->blockNr = 0;
  x->file = file;
  insertInTodo(todo, x);
}

/* Look for matches of sum (i.e. scanImage()'s rsum). If found, insert
   appropriate entry in todo list. Returns true if nextEvent may have
   been modified. */
inline void MkTemplate::checkRsyncSumMatch(const RsyncSum64& sum,
    const uint32& bitMask, const size_t blockLen, const size_t back,
    const uint64 off, const size_t md5BlockLength, PartialMatch*& todo,
    PartialMatch*& todoFree, uint64& nextEvent,const uint64 unmatchedStart) {

  typedef const vector<FilePart*> FVec;
  FVec& hashEntry = block[sum.getHi() & bitMask];
  if (hashEntry.empty()) return;

  FVec::const_iterator i = hashEntry.begin(), e = hashEntry.end();
  do {
    /* If todoFree == 0 (no more space left in queue), just discard
       this possible file match! It's the only option if there are
       many, many overlapping matches, otherwise the program would get
       extremely slow. */
#   if DEBUG_MKTEMPLATE
    if (todoFree == 0) {
      static FilePart* file = 0;
      if (file != *i) {
        file = *i;
        cerr << ' ' << off << ": DROPPED possible " << file->leafName()
             << " match(es) at offset " << off - blockLen << "+ (todo full)"
             << endl;
      }
    }
#   endif
    if (todoFree == 0) return;
    FilePart* file = *i;
    const RsyncSum64* fileSum = file->getRsyncSum(cache);
    if (fileSum != 0 && *fileSum == sum)
      checkRsyncSumMatch2(blockLen, back, off, md5BlockLength, todo,
                          todoFree, nextEvent, unmatchedStart, file);
    ++i;
  } while (i != e);
  return;
}
//________________________________________

// Read the 'count' first bytes from file x and write them to zip
bool MkTemplate::rereadUnmatched(const PartialMatch* x, uint64 count,
                                 Zobstream& zip) {
  // Lower peak memory usage: Deallocate cache's buffer
  cache->deallocBuffer();

  ArrayAutoPtr<byte> tmpBuf(new byte[readAmount]);
  string inputName = x->file->getPath();
  inputName += x->file->leafName();
  auto_ptr<bistream> inputFile(new bifstream(inputName.c_str(),ios::binary));
  while (inputFile->good() && count > 0) {
    // read data
    readBytes(*inputFile, tmpBuf.get(),
              (readAmount < count ? readAmount : count));
    size_t n = inputFile->gcount();
    zip.write(tmpBuf.get(), n); // will catch Zerror "upstream"
    Paranoid(n <= count);
    count -= n;
  }
  if (count == 0) return SUCCESS;

  // error
  string err = subst(_("Error while reading from file `%1' (%2)"),
                     inputName, strerror(errno));
  reporter.error(err);
  return FAILURE;
}
//________________________________________

#if DEBUG
// Print info about a part of the input image
#  if DEBUG_MKTEMPLATE
void MkTemplate::debugRangeInfo(uint64 start, uint64 end, const char* msg,
                                const PartialMatch* x) {
  Assert(oldAreaEnd == start);
  cerr << '[' << start << ',' << end << ") " << msg;
  if (x != 0) cerr << ' ' << x->file->leafName();
  cerr << endl;
  oldAreaEnd = end;
}
#  else
void MkTemplate::debugRangeInfo(uint64 start, uint64 end, const char*,
                                const PartialMatch*) {
  Assert(oldAreaEnd == start);
  oldAreaEnd = end;
}
#  endif
#endif
//________________________________________

/* Calculate MD5 for the previous md5ChunkLength (or less if at end of
   match) bytes. If the calculated checksum matches and it is the last
   MD5 block in the file, record a file match. If the i-th MD5Sum does
   not match, write the i*md5ChunkLength bytes directly to templ.
   @param stillBuffered bytes of image data "before current position"
   that are still in the buffer; they are at buf[data] to
   buf[(data+stillBuffered-1)%bufferLength] */
inline bool MkTemplate::checkMD5Match(const uint64 off, byte* const buf,
    const size_t bufferLength, const size_t data, PartialMatch*& todo,
    PartialMatch*& todoFree, const size_t md5BlockLength, uint64& nextEvent,
    Zobstream& zip, const size_t stillBuffered, Desc& desc,
    uint64& unmatchedStart, set<FilePart*>& matched) {
  // Assert(todo != 0 && todo->nextEvent == off) - cf. how this is called
  PartialMatch* x = todo;
  todo = todo->next;

  /* Calculate MD5Sum from buf[x->blockOff] to buf[data-1], deal with
     wraparound. NB 0 <= x->blockOff < bufferLength, but 1 <= data <
     bufferLength+1 */
  static MD5Sum md;
  md.reset();
  if (x->blockOff < data) {
    md.update(buf + x->blockOff, data - x->blockOff);
  } else {
    md.update(buf + x->blockOff, bufferLength - x->blockOff);
    md.update(buf, data);
  }
  md.finishForReuse();
  //____________________

  const MD5* xfileSum = x->file->getSums(cache, x->blockNr);
# if DEBUG_MKTEMPLATE
  cerr << "checkMD5Match?: image " << md << ", file "
       << x->file->leafName() << " block #"
       << x->blockNr << ' ';
  if (xfileSum) cerr << *xfileSum; else cerr << "[error]";
  cerr << endl;
# endif
  if (xfileSum == 0 || md != *xfileSum) {
    /* The block didn't match, so the whole file doesn't match -
       re-read from file any data that is no longer buffered, and
       write it to the Zobstream. */
    x->next = todoFree;
    todoFree = x; // return x to free pool

    uint64 rereadEnd = off - stillBuffered;
    if (todo != 0 && todo->startOff < rereadEnd) rereadEnd = todo->startOff;
    if (rereadEnd <= x->startOff) {
      // everything still buffered, or there is another pending match
      return SUCCESS;
    }
    debugRangeInfo(x->startOff, rereadEnd,
                   "UNMATCHED after some blocks, re-reading from", x);
    desc.unmatchedData(rereadEnd - x->startOff);
    unmatchedStart = rereadEnd;
    return rereadUnmatched(x, rereadEnd - x->startOff, zip);
  } // endif (file doesn't match)
  //____________________

  // Another block of file matched - was it the last one?
  if (off < x->startOff + x->file->size()) {
    // Still some more to go - update x and re-insert in todo
    x->nextEvent = min(x->nextEvent + md5BlockLength,
                       x->startOff + x->file->size());
    nextEvent = min(nextEvent, x->nextEvent);
#   if DEBUG_MKTEMPLATE
    cerr << "checkMD5Match: match and more to go, next at off "
         << x->nextEvent << endl;
#   endif
    x->blockOff = data;
    ++x->blockNr;
    insertInTodo(todo, x);
    return SUCCESS;
  }
  //____________________

  Assert(off == x->startOff + x->file->size());
  // Heureka! *MATCH*
  // x = address of PartialMatch obj of file that matched
  reporter.matchFound(x->file, x->startOff);
  matched.insert(x->file);

  /* Remove all entries in todo with startOff < off. This is the
     greedy approach and is not ideal: If a small file A happens to
     match one small fraction of a large file B and B is contained in
     the image, then only a match of A will be found. */
  const PartialMatch* y = x;
  for (PartialMatch* i = x; i->next != 0; ) {
    PartialMatch* tmp = i->next;
    if (tmp->startOff < off) {
      // y = address of PartialMatch with lowest startOff
      if (tmp->startOff < y->startOff) y = tmp;
      i->next = tmp->next;
      tmp->next = todoFree;
      todoFree = tmp;
    } else {
      i = tmp;
    }
  }

  /* Re-read and write out data before the start of the match, i.e. of
     any half-finished bigger match (which we abandon now that we've
     found the small match). */
  if (x != y && y->startOff < off - stillBuffered) {
    unmatchedStart = min(off - stillBuffered, x->startOff);
    debugRangeInfo(y->startOff, unmatchedStart,
                   "UNMATCHED, re-reading partial match from", y);
    size_t toReread = unmatchedStart - y->startOff;
    desc.unmatchedData(toReread);
    if (rereadUnmatched(y, toReread, zip))
      return FAILURE;
  }
  /* Write out data that is still buffered, and is before the start of
     the match. */
  if (unmatchedStart < x->startOff) {
    debugRangeInfo(unmatchedStart, x->startOff,
                   "UNMATCHED, buffer flush before match");
    size_t toWrite = x->startOff - unmatchedStart;
    Paranoid(off - unmatchedStart <= bufferLength);
    size_t writeStart = modSub(data, off - unmatchedStart, bufferLength);
    writeBuf(buf, writeStart, modAdd(writeStart, toWrite, bufferLength),
             bufferLength, zip);
    desc.unmatchedData(toWrite);
  }

  // Assert(x->file->mdValid);
  desc.matchedFile(x->file->size(), *(x->file->getRsyncSum(cache)),
                   *(x->file->getMD5Sum(cache)));
  unmatchedStart = off;
  debugRangeInfo(x->startOff, off, "MATCH:", x);

  todo = x->next;
  x->next = todoFree; // Remove matching entry
  todoFree = x;

# if DEBUG /* check consistency of todo queue */
  int todoCount = 0;
  for (PartialMatch* i = todo; i != 0 && todoCount <= TODO_MAX;
       i = i->next) {
    Assert(i->next == 0 || *i <= *(i->next));
    ++todoCount;
  }
  for (PartialMatch* i = todoFree; i != 0 && todoCount <= TODO_MAX;
       i = i->next) ++todoCount;
  Assert(todoCount == TODO_MAX);
# endif
  return SUCCESS;
}
//________________________________________

/* This is called in case the end of the image has been reached, but
   unmatchedStart < off, i.e. there was a partial match at the end of
   the image. Just discards that match and writes the data to the
   template, either by re-reading from the partially matched file, or
   from the buffer. Compare to similar code in checkMD5Match.

   Since we are at the end of the image, the full last bufferLength
   bytes of the image are in the buffer. */
inline bool MkTemplate::unmatchedAtEnd(const uint64 off, byte* const buf,
    const size_t bufferLength, const size_t data, PartialMatch*& todo,
    Zobstream& zip, Desc& desc, uint64& unmatchedStart) {
  Paranoid(unmatchedStart < off); // cf. where this is called

  // Find entry in todo with (startOff == unmatchedStart), if any
  const PartialMatch* y = 0;
  for (PartialMatch* i = todo; i != 0; i = i->next)
    if (i->startOff == unmatchedStart) y = i;

  // Re-read and write out data that is no longer buffered.
  if (y != 0 && y->startOff < off - bufferLength) {
    unmatchedStart = off - bufferLength;
    debugRangeInfo(y->startOff, unmatchedStart,
                   "UNMATCHED at end, re-reading partial match from", y);
    size_t toReread = unmatchedStart - y->startOff;
    desc.unmatchedData(toReread);
    if (rereadUnmatched(y, toReread, zip))
      return FAILURE;
  }
  // Write out data that is still buffered
  if (unmatchedStart < off) {
    debugRangeInfo(unmatchedStart, off, "UNMATCHED at end");
    size_t toWrite = off - unmatchedStart;
    Assert(toWrite <= bufferLength);
    size_t writeStart = modSub(data, toWrite, bufferLength);
    writeBuf(buf, writeStart, data, bufferLength, zip);
    desc.unmatchedData(toWrite);
    unmatchedStart = off;
  }
  return SUCCESS;
}
//________________________________________

/* Scan image. Central function for template generation.

   Treat buf as a circular buffer. Read new data into at most half the
   buffer. Calculate a rolling checksums covering blockLength bytes.
   When it matches an entry in block, start calculating MD5Sums of
   blocks of length md5BlockLength.

   Since both image and templ can be non-seekable, we run into a
   problem in the following case: After the initial RsyncSum match, a
   few of the md5BlockLength-sized chunks of one input file were
   matched, but not all, so in the end, there is no match.
   Consequently, we would now need to re-read that part of the image
   and pump it through zlib to templ - but we can't if the image is
   stdin! Solution: Since we know that the MD5Sum of a block matched
   part of an input file, we can re-read from there. */
inline bool MkTemplate::scanImage(byte* buf, size_t bufferLength,
    size_t blockLength, uint32 blockMask, size_t md5BlockLength,
    set<FilePart*>& matched) {
  bool result = SUCCESS;

  /* Cause input files to be analysed */
  if (scanFiles(blockLength, blockMask, md5BlockLength))
    result = FAILURE;

  /* Initialise rolling sums with blockSize bytes 0x7f, and do the
     same with part of buffer, to avoid special-case code in main
     loop. (Any value would do - except that 0x00 or 0xff might lead
     to a larger number of false positives.) */
  RsyncSum64 rsum;
  byte* bufEnd = buf + bufferLength;
  for (byte* z = bufEnd - blockLength; z < bufEnd; ++z) *z = 0x7f;
  rsum.addBackNtimes(0x7f, blockLength);

  // Compression pipe for templ data
  Zobstream zip(*templ, ZIPCHUNK_SIZE, zipQual);
  Desc desc; // Buffer for DESC data, appended to templ at end
  size_t data = 0; // Offset into buf of byte currently being processed
  uint64 off = 0; // Offset in image, of "data"
  uint64 nextReport = 0; // call reporter once off reaches this value

  /* The area delimited by unmatchedStart (incl) and off (excl) has
     "not been dealt with", either by writing it to zip, or by a match
     with the first MD5 block of an input file. */
  uint64 unmatchedStart = 0;

  MD5Sum imageMd5Sum; // MD5 of whole image
  MD5Sum md; // Re-used for each 2nd-level check of any rsum match

  ArrayAutoPtr<PartialMatch> todoDel(new PartialMatch[TODO_MAX]);
  PartialMatch* todo = todoDel.get();
  PartialMatch* todoFree = todo;
  todoFree->next = 0;
  for (int i = 0; i < TODO_MAX - 1; ++i) {
    (++todoFree)->next = todo; ++todo; // Link all elements to todoFree
  }
  todo = 0;

  // Read image
  size_t rsumBack = bufferLength - blockLength;

  try {
    /* Catch Zerrors, which can occur in zip.write(), writeBuf(),
       checkMD5Match(), zip.close() */
    while (image->good()) {

#     if DEBUG_MKTEMPLATE
      cerr << "---------- main loop. off=" << off << " data=" << data
           << " unmatchedStart=" << unmatchedStart << endl;
#     endif

      if (off >= nextReport) { // Keep user entertained
        reporter.scanningImage(off);
        nextReport += REPORT_INTERVAL;
      }

      // zip().write() out any old data that we'll destroy with read()
      size_t thisReadAmount = (readAmount < bufferLength - data ?
                               readAmount : bufferLength - data);
      uint64 lowestStartOff = 0;
      if (off > blockLength) lowestStartOff = off - blockLength;
      for (PartialMatch* q = todo; q != 0; q = q->next)
        if (q->startOff < lowestStartOff) lowestStartOff = q->startOff;
      if (unmatchedStart < lowestStartOff) {
        size_t toWrite = lowestStartOff - unmatchedStart;
        Paranoid(off - unmatchedStart <= bufferLength);
        size_t writeStart = modSub(data, off - unmatchedStart,
                                   bufferLength);
        debugRangeInfo(unmatchedStart, unmatchedStart + toWrite,
                       "UNMATCHED");
        writeBuf(buf, writeStart,
                 modAdd(writeStart, toWrite, bufferLength),
                 bufferLength, zip);
        unmatchedStart += toWrite;
        desc.unmatchedData(toWrite);
      }

      // Read new data from image
#     if DEBUG // just for testing, make it sometimes read less
      static size_t chaosOff, acc;
      if (unmatchedStart == 0) { chaosOff = 0; acc = 0; }
      acc += buf[chaosOff] ^ buf[off % bufferLength] + ~off;
      if (chaosOff == 0) chaosOff = bufferLength - 1; else --chaosOff;
      if ((acc & 0x7) == 0) {
        thisReadAmount = acc % thisReadAmount + 1;
#       if DEBUG_MKTEMPLATE
        cerr << "debug: thisReadAmount=" << thisReadAmount << endl;
#       endif
      }
#     endif
      readBytes(*image, buf + data, thisReadAmount);
      size_t n = image->gcount();
      imageMd5Sum.update(buf + data, n);

      while (n > 0) { // Still unprocessed bytes left
        uint64 nextEvent = off + n; // Special event: end of buffer
        if (todo != 0) nextEvent = min(nextEvent, todo->nextEvent);

        /* Unrolled innermost loop - see below for single-iteration
           version. Also see checkRsyncSumMatch above. */
        while (off < nextEvent - 32 && rsumBack < bufferLength - 32) {
          size_t dataOld = data;
          do {
            const vector<FilePart*>* hashEntry;
#           define x ; \
            rsum.removeFront(buf[rsumBack], blockLength); \
            rsum.addBack(buf[data]); \
            ++data; ++rsumBack; \
            hashEntry = &block[rsum.getHi() & blockMask]; \
            if (hashEntry->size() > 1) break; \
            if (hashEntry->size() == 1) { \
              FilePart* file = (*hashEntry)[0]; \
              const RsyncSum64* fileSum = file->getRsyncSum(cache); \
              if (fileSum != 0 && *fileSum == rsum) break; \
            }
            x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x
#           undef x
            dataOld = data; // Special case: "Did not break out of loop"
          } while (false);
          if (dataOld == data) {
            off += 32; n -= 32;
          } else {
            dataOld = data - dataOld; off += dataOld; n -= dataOld;
            checkRsyncSumMatch(rsum, blockMask, blockLength, rsumBack,
                               off, md5BlockLength, todo, todoFree,
                               nextEvent, unmatchedStart);
          }
        }

        // Innermost loop - single-byte version
        while (off < nextEvent) {
          // Roll checksums by one byte
          rsum.removeFront(buf[rsumBack], blockLength);
          rsum.addBack(buf[data]);
          ++data; ++off; --n;
          rsumBack = modAdd(rsumBack, 1, bufferLength);

          /* Look for matches of rsum. If found, insert appropriate
             entry in todo list and maybe modify nextEvent. */
          checkRsyncSumMatch(rsum, blockMask, blockLength, rsumBack, off,
                             md5BlockLength, todo, todoFree, nextEvent,
                             unmatchedStart);

          /* We mustn't by accident schedule an event for a part of
             the image that has already been flushed out of the
             buffer/matched */
          Paranoid(todo == 0 || todo->startOff >= unmatchedStart);
        }
#       if DEBUG_MKTEMPLATE
        cerr << ' ' << off << ": Event, todoOff=";
        if (todo == 0) cerr << "(none)"; else cerr << todo->nextEvent;
        cerr << endl;
#       endif

        /* Calculate MD5 for the previous md5ChunkLength (or less if
           at end of match) bytes, if necessary. If the calculated
           checksum matches and it is the last MD5 block in the file,
           record a file match. If the i-th MD5Sum does not match,
           write the i*md5ChunkLength bytes directly to templ. */
        while (todo != 0 && todo->nextEvent == off) {
          size_t stillBuffered = bufferLength - n;
          if (stillBuffered > off) stillBuffered = off;
          if (checkMD5Match(off, buf, bufferLength, data, todo, todoFree,
                     md5BlockLength, nextEvent, zip, stillBuffered, desc,
                     unmatchedStart, matched))
            return FAILURE; // no recovery possible, exit immediately
        }

        Assert(todo == 0 || todo->nextEvent > off);
#       if DEBUG /* check consistency of todo queue */
        int todoCount = 0;
        for (PartialMatch* i = todo; i != 0 && todoCount <= TODO_MAX;
             i = i->next) {
          Assert(i->next == 0 || *i <= *(i->next));
          ++todoCount;
        }
        for (PartialMatch* i = todoFree; i != 0 && todoCount <= TODO_MAX;
             i = i->next) ++todoCount;
        Assert(todoCount == TODO_MAX);
#       endif
      } // endwhile (n > 0), i.e. more unprocessed bytes left

      if (data == bufferLength) data = 0;
      Assert(data < bufferLength);

    } // endwhile (image->good())

    // End of image data - any remaining partial match is UNMATCHED
    if (unmatchedStart < off
        && unmatchedAtEnd(off, buf, bufferLength, data, todo, zip, desc,
                          unmatchedStart)) return FAILURE;
    Assert(unmatchedStart == off);

    zip.close();
  }
  catch (Zerror ze) {
    string err = subst(_("Error during compression: %1"), ze.message);
    reporter.error(err);
    return FAILURE;
  }

  imageMd5Sum.finish();
  desc.imageInfo(off, imageMd5Sum, cache->getBlockLen());
  desc.put(*templ);
  if (!*templ) {
    string err = _("Could not write template data");
    reporter.error(err);
    return FAILURE;
  }

  reporter.finished(off);
  return result;
}
//______________________________________________________________________

// Write contents of .jigdo file
// matched: Pointers to files that were found in image
bool MkTemplate::writeJigdo(set<FilePart*>& matched,
                            const string& imageLeafName) {
  /* For recording which ones are used by the files in the image. Maps
     from label to LocationPath */
  typedef map<string, LocationPathSet::iterator> ServerMap;
  ServerMap servers;
  //____________________

  /* Build list of locations that are a) used by elements of matched,
     and b) already have a label */
  for (set<FilePart*>::iterator i = matched.begin(), e = matched.end();
       i != e; ++i) {
    LocationPathSet::iterator loc = (*i)->getLocation();
    //cerr << "Label " << loc->getPath() << ' ' << loc->getLabel() << endl;
    if (!loc->getLabel().empty())
      servers.insert(make_pair(loc->getLabel(), loc));
  }
  //____________________

  // Output file header
  *jigdo << subst(_(
    "# JigsawDownload\n"
    "# See %1 for details about jigdo\n"
    "\n"
    "# You can gzip this file - afterwards, remove the gz extension,\n"
    "# jigdo will automatically unzip it.\n"
    "\n"
    "# Selected=yes|no - should image be selected for download by default?\n"
    "# ShortInfo: Short description of image (1 line), for menus etc.\n"
    "# Info: Longer, lines may be rewrapped.\n"
    "\n"
    "# There is one [Image] section for each image.\n"
    "\n"), URL);
  *jigdo <<
    "[Jigdo]\nVersion="
         << FILEFORMAT_MAJOR << '.' << FILEFORMAT_MINOR
         << "\nGenerator=jigdo-file/" JIGDO_VERSION
         << "\nInfo=\n";
  //____________________

  if (addImageSection) {
    // Output info about image
    *jigdo << "\n[Image]\nFilename=" << imageLeafName
           << "\nTemplate=http://edit.this.url/" << imageLeafName
           << ".template\nSelected=\nShortInfo=\nInfo=\n";
  }
  //____________________

  /* Output information about files in matched. For any remaining
     locations, assign a label when entering the location into
     servers. */
  *jigdo << "\n[Parts]\n";
  string locName = "A";
  for (set<FilePart*>::iterator i = matched.begin(), e = matched.end();
       i != e; ++i) {
    LocationPathSet::iterator loc = (*i)->getLocation();
    if (loc->getLabel().empty()) {
      // Location has not been --label'led, so just generate a name
      while (servers.find(locName) != servers.end()) {
        size_t n = 0;
        while (++locName[n] == 'Z' + 1) { // "Increment locName"
          locName[n] = 'A';
          ++n;
          if (n == locName.size()) { locName.append(1, 'A'); break; }
        }
      }
      // Insert into list of servers to output below
      servers.insert(make_pair(locName, loc));
      // Why TF does a simple loc->setLabel(locName) cause a "is const" error
      const_cast<LocationPath&>(*loc).setLabel(locName);
    }
    // Output file info
    string uri = loc->getLabel();
    uri += ':';
    uri += (*i)->leafName();
    if (DIRSEP != '/') compat_swapFileUriChars(uri);
    ConfigFile::quote(uri);
    (*jigdo) << *((*i)->getMD5Sum(cache)) << '=' << uri << '\n';
  }
  //____________________

  // Output information about locations used
  if (addServersSection) {
    (*jigdo) << "\n[Servers]\n";
    for (ServerMap::iterator i = servers.begin(), e = servers.end();
         i != e; ++i) {
      (*jigdo) << i->first << '=' << i->second->getUri() << '\n';
    }
  }

  return SUCCESS;
}
//______________________________________________________________________

// Central function which processes the data and finds matches
bool MkTemplate::run(const string& imageLeafName) {
  bool result = SUCCESS;

  oldAreaEnd = 0;

  // Kick out files that are too small
  for (JigdoCache::iterator f = cache->begin(), e = cache->end();
       f != e; ++f) {
    if (f->size() < cache->getBlockLen()) {
      f->markAsDeleted(cache);
      continue;
    }
    fileSizeTotal += f->size();
    ++fileCount;
  }

  // Hash table performance drops when linked lists become long => "+1"
  int    blockBits = bitWidth(fileCount) + 1;
  uint32 blockMask = (1 << blockBits) - 1;
  block.resize(blockMask + 1);

  size_t max_MD5Len_blockLen =
      cache->getBlockLen() + 64; // +64 for Assert below
  if (max_MD5Len_blockLen < cache->getMD5BlockLen())
    max_MD5Len_blockLen = cache->getMD5BlockLen();
  /* Pass 1 imposes no minimum buffer length, only pass 2: Must always
     be able to read readAmount bytes into one buffer half in one go;
     must be able to start calculating an MD5Sum at a position that is
     blockLength bytes back in input; must be able to write out at
     least previous md5BlockLength bytes in case there is no match. */
  size_t bufferLength = 2 *
    (max_MD5Len_blockLen > readAmount ? max_MD5Len_blockLen : readAmount);
  // Avoid reading less bytes than readAmount at any time
  bufferLength = (bufferLength + readAmount - 1) / readAmount * readAmount;

  Paranoid(bufferLength % readAmount == 0); // for efficiency only
  // Asserting this makes things easier in pass 2. Yes it is ">" not ">="
  Assert(cache->getMD5BlockLen() > cache->getBlockLen());

# if DEBUG_MKTEMPLATE
  cerr << "nr of files: " << fileCount << " (" << blockBits << " bits)\n"
       << "total bytes: " << fileSizeTotal << '\n'
       << "blockLength: " << cache->getBlockLen() << '\n'
       << "md5BlockLen: " << cache->getMD5BlockLen() << '\n'
       << "bufLen (kB): " << bufferLength/1024 << '\n'
       << "zipQual:     " << zipQual << endl;
# endif

  set<FilePart*> matched; // Pointers to files that are found in image

  ArrayAutoPtr<byte> bufDel(new byte[bufferLength]);
  byte* buf = bufDel.get();

  // Write header to template file
  (*templ) << TEMPLATE_HDR << FILEFORMAT_MAJOR << '.'
           << FILEFORMAT_MINOR << " jigdo-file/"JIGDO_VERSION
           << "\r\nSee " << URL << " for details about jigdo.\r\n\r\n"
           << flush;
  if (!*templ) result = FAILURE;

  // Read input image and output parts that do not match
  if (scanImage(buf, bufferLength, cache->getBlockLen(), blockMask,
                cache->getMD5BlockLen(), matched)) {
    result = FAILURE;
  }
  cache->deallocBuffer();

  if (writeJigdo(matched, imageLeafName)) result = FAILURE;

# if DEBUG_MKTEMPLATE
  cerr << "MkTemplate::run() finished" << endl;
# endif
  return result;
}
