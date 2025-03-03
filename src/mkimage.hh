/* $Id: mkimage.hh,v 1.3 2004/09/12 21:08:28 atterer Exp $ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2002  |  richard@
  | \/�|  Richard Atterer          |  atterer.org
  � '` �
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

*//** @file

  Create image from template / merge new files into image.tmp

*/

#ifndef MKIMAGE_HH
#define MKIMAGE_HH

#include <config.h>

#include <iosfwd>
#include <queue>
#include <vector>
#include <typeinfo>

#include <bstream.hh>
#include <debug.hh>
#include <md5sum.hh>
#include <sha256sum.hh>
#include <scan.hh>
#include <serialize.hh>
//______________________________________________________________________

/** Errors thrown by the JigdoDesc code */
struct JigdoDescError : Error {
  explicit JigdoDescError(const string& m) : Error(m) { }
  explicit JigdoDescError(const char* m) : Error(m) { }
};

/** Entry in a DESC section of a jigdo template. This definition is
    used both for an abstract base class and as a namespace for child
    classes. */
class JigdoDesc {
public:
  /** Types of entries in a description section */
  enum Type {
    OBSOLETE_IMAGE_INFO = 1,
    UNMATCHED_DATA = 2,
    OBSOLETE_MATCHED_FILE = 3,
    OBSOLETE_WRITTEN_FILE = 4,
    IMAGE_INFO_MD5 = 5,
    MATCHED_FILE_MD5 = 6,
    WRITTEN_FILE_MD5 = 7,
    IMAGE_INFO_SHA256 = 8,
    MATCHED_FILE_SHA256 = 9,
    WRITTEN_FILE_SHA256 = 10
  };
  class ProgressReporter;
  //____________________

  virtual bool operator==(const JigdoDesc& x) const = 0;
  inline bool operator!=(const JigdoDesc& x) const { return !(*this == x); }
  virtual ~JigdoDesc() = 0;

  /** Entry type of JigdoDesc child class */
  virtual Type type() const = 0;
  /** Output human-readable summary */
  virtual ostream& put(ostream& s) const = 0;
  /** Size of image area or whole image */
  virtual uint64 size() const = 0;

  /** There are no virtual templates, so this wouldn't work:<code>
      template<class Iterator>
      virtual Iterator serialize(Iterator i) const; </code> */
  virtual size_t serialSizeOf() const = 0;

  /** Check whether the file is a .template file, i.e. has the
      appropriate ASCII header */
  static bool isTemplate(bistream& file);
  /** Assuming that a DESC section is at the end of a file, set the
      file pointer to the start of the section, allowing you to call
      read() immediately afterwards. */
  static void seekFromEnd(bistream& file);
  /** Create image file from template and files (via JigdoCache) */
  static int makeImage(JigdoCache* cache, const string& imageFile,
    const string& imageTmpFile, const string& templFile,
    bistream* templ, const bool optForce,
    ProgressReporter& pr = noReport, size_t readAmnt = 128U*1024,
    const bool optMkImageCheck = true);
  /** Return list of MD5sums of files that still need to be copied to
      the image to complete it. Reads info from tmp file or (if
      imageTmpFile.empty() or error opening tmp file) outputs complete
      list from template. */
  static int listMissingMD5(set<MD5>& result, const string& imageTmpFile,
    const string& templFile, bistream* templ, ProgressReporter& reporter);
  /** Return list of SHA256sums of files that still need to be copied to
      the image to complete it. Reads info from tmp file or (if
      imageTmpFile.empty() or error opening tmp file) outputs complete
      list from template. */
  static int listMissingSHA256(set<SHA256>& result, const string& imageTmpFile,
    const string& templFile, bistream* templ, ProgressReporter& reporter);

  class ImageInfoMD5;
  class ImageInfoSHA256;
  class UnmatchedData;
  class MatchedFileMD5;
  class MatchedFileSHA256;
  class WrittenFileMD5;
  class WrittenFileSHA256;

private:
  static ProgressReporter noReport;
};

inline ostream& operator<<(ostream& s, JigdoDesc& jd) { return jd.put(s); }
//______________________________________________________________________

/** Information about the image file */
class JigdoDesc::ImageInfoMD5 : public JigdoDesc {
public:
  inline ImageInfoMD5(uint64 s, const MD5& m, size_t b);
  inline ImageInfoMD5(uint64 s, const MD5Sum& m, size_t b);
  inline bool operator==(const JigdoDesc& x) const;
  Type type() const { return IMAGE_INFO_MD5; }
  uint64 size() const { return sizeVal; }
  const MD5& md5() const { return md5Val; }
  size_t blockLength() const { return blockLengthVal; }
  // Default dtor, operator==
  virtual ostream& put(ostream& s) const;

  template<class Iterator>
  inline Iterator serialize(Iterator i) const;
  inline size_t serialSizeOf() const;

private:
  uint64 sizeVal;
  MD5 md5Val;
  size_t blockLengthVal;
};
//________________________________________

/** Information about the image file */
class JigdoDesc::ImageInfoSHA256 : public JigdoDesc {
public:
  inline ImageInfoSHA256(uint64 s, const SHA256& m, size_t b);
  inline ImageInfoSHA256(uint64 s, const SHA256Sum& m, size_t b);
  inline bool operator==(const JigdoDesc& x) const;
  Type type() const { return IMAGE_INFO_SHA256; }
  uint64 size() const { return sizeVal; }
  const SHA256& sha256() const { return sha256Val; }
  size_t blockLength() const { return blockLengthVal; }
  // Default dtor, operator==
  virtual ostream& put(ostream& s) const;

  template<class Iterator>
  inline Iterator serialize(Iterator i) const;
  inline size_t serialSizeOf() const;

private:
  uint64 sizeVal;
  SHA256 sha256Val;
  size_t blockLengthVal;
};
//________________________________________

/** Info about data that was not matched by any input file, i.e.
    that is included in the template data verbatim */
class JigdoDesc::UnmatchedData : public JigdoDesc {
public:
  UnmatchedData(uint64 o, uint64 s) : offsetVal(o), sizeVal(s) { }
  inline bool operator==(const JigdoDesc& x) const;
  Type type() const { return UNMATCHED_DATA; }
  uint64 offset() const { return offsetVal; }
  uint64 size() const { return sizeVal; }
  void resize(uint64 s) { sizeVal = s; }
  // Default dtor, operator==
  virtual ostream& put(ostream& s) const;

  template<class Iterator>
  inline Iterator serialize(Iterator i) const;
  inline size_t serialSizeOf() const;

private:
  uint64 offsetVal; // Offset in image
  uint64 sizeVal;
};
//________________________________________

/** Info about data that *was* matched by an input file */
class JigdoDesc::MatchedFileMD5 : public JigdoDesc {
public:
  inline MatchedFileMD5(uint64 o, uint64 s, const RsyncSum64& r, const MD5& m);
  inline MatchedFileMD5(uint64 o, uint64 s, const RsyncSum64& r,
                     const MD5Sum& m);
  inline bool operator==(const JigdoDesc& x) const;
  Type type() const { return MATCHED_FILE_MD5; }
  uint64 offset() const { return offsetVal; }
  uint64 size() const { return sizeVal; }
  const MD5& md5() const { return md5Val; }
  const RsyncSum64& rsync() const { return rsyncVal; }
  // Default dtor, operator==
  virtual ostream& put(ostream& s) const;

  template<class Iterator>
  inline Iterator serialize(Iterator i) const;
  inline size_t serialSizeOf() const;

private:
  uint64 offsetVal; // Offset in image
  uint64 sizeVal;
  RsyncSum64 rsyncVal;
  MD5 md5Val;
};
//________________________________________

/** Like MatchedFileMD5 - used only in .tmp files to express that the
    file data was successfully written to the image. NB: Because this
    derives from MatchedFileMD5 and because of the implementation of
    JigdoDesc::operator==, MatchedFileMD5's and WrittenFileMD5's will
    compare equal if their data fields are identical. */
class JigdoDesc::WrittenFileMD5 : public MatchedFileMD5 {
public:
  WrittenFileMD5(uint64 o, uint64 s, const RsyncSum64& r, const MD5& m)
    : MatchedFileMD5(o, s, r, m) { }
  // Implicit cast to allow MatchedFileMD5 and WrittenFileMD5 to compare equal
  inline bool operator==(const JigdoDesc& x) const;
  Type type() const { return WRITTEN_FILE_MD5; }
  virtual ostream& put(ostream& s) const;

  template<class Iterator>
  inline Iterator serialize(Iterator i) const;
  inline size_t serialSizeOf() const;
};
//______________________________________________________________________

/** Info about data that *was* matched by an input file */
class JigdoDesc::MatchedFileSHA256 : public JigdoDesc {
public:
  inline MatchedFileSHA256(uint64 o, uint64 s, const RsyncSum64& r, const SHA256& m);
  inline MatchedFileSHA256(uint64 o, uint64 s, const RsyncSum64& r,
                     const SHA256Sum& m);
  inline bool operator==(const JigdoDesc& x) const;
  Type type() const { return MATCHED_FILE_SHA256; }
  uint64 offset() const { return offsetVal; }
  uint64 size() const { return sizeVal; }
  const SHA256& sha256() const { return sha256Val; }
  const RsyncSum64& rsync() const { return rsyncVal; }
  // Default dtor, operator==
  virtual ostream& put(ostream& s) const;

  template<class Iterator>
  inline Iterator serialize(Iterator i) const;
  inline size_t serialSizeOf() const;

private:
  uint64 offsetVal; // Offset in image
  uint64 sizeVal;
  RsyncSum64 rsyncVal;
  SHA256 sha256Val;
};
//________________________________________

/** Like MatchedFileSHA256 - used only in .tmp files to express that the
    file data was successfully written to the image. NB: Because this
    derives from MatchedFileSHA256 and because of the implementation of
    JigdoDesc::operator==, MatchedFileSHA256's and WrittenFileSHA256's will
    compare equal if their data fields are identical. */
class JigdoDesc::WrittenFileSHA256 : public MatchedFileSHA256 {
public:
  WrittenFileSHA256(uint64 o, uint64 s, const RsyncSum64& r, const SHA256& m)
    : MatchedFileSHA256(o, s, r, m) { }
  // Implicit cast to allow MatchedFileSHA256 and WrittenFileSHA256 to compare equal
  inline bool operator==(const JigdoDesc& x) const;
  Type type() const { return WRITTEN_FILE_SHA256; }
  virtual ostream& put(ostream& s) const;

  template<class Iterator>
  inline Iterator serialize(Iterator i) const;
  inline size_t serialSizeOf() const;
};
//______________________________________________________________________

/** Class allowing JigdoDesc to convey information back to the caller.
    The default versions of the methods do nothing at all (except for
    error(), which prints the error to cerr) - you need to supply an
    object of a derived class to functions to get called back. */
class JigdoDesc::ProgressReporter {
public:
  virtual ~ProgressReporter() { }
  /** General-purpose error reporting. */
  virtual void error(const string& message);
  /** Like error(), but for purely informational messages. */
  virtual void info(const string& message);
  /** Called when the output image (or a temporary file) is being
      written to. It holds that written==imgOff and
      totalToWrite==imgSize, *except* when additional files are merged
      into an already existing temporary file.
      @param written Number of bytes written so far
      @param totalToWrite Value of 'written' at end of write operation
      @param imgOff Current offset in image
      @param imgSize Total size of output image */
  virtual void writingImage(uint64 written, uint64 totalToWrite,
                            uint64 imgOff, uint64 imgSize);
};
//______________________________________________________________________

/** Container for JidoDesc objects. Is mostly a vector<JidoDesc*>, but
    deletes elements when destroyed. However, when removing elements,
    resizing the JigdoDescVec etc., the elements are *not* deleted
    automatically. */
class JigdoDescVec : public vector<JigdoDesc*> {
public:
  JigdoDescVec() : vector<JigdoDesc*>() { }
  inline ~JigdoDescVec();

  /** Read JigdoDescs from a template file into *this. *this is
      clear()ed first. File pointer must be at start of first entry;
      the "DESC" must have been read already. If error is thrown,
      position of file pointer is undefined. A type 1 (IMAGE_INFO_MD5)
      will end up at this->back(). */
  bistream& get(bistream& file);

  /** Write a DESC section to a binary stream. Note that there should
      not be two contiguous Unmatched regions - this is not checked.
      Similarly, the length of the ImageInfo* part must match the
      accumulated lengths of the other parts. */
  bostream& put(bostream& file, MD5Sum* md = 0, SHA256Sum* sd = 0, int checksumChoice = 0) const;

  /** List contents of a JigdoDescVec to a stream in human-readable format. */
  void list(ostream& s) throw();
private:
  // Disallow copying (too inefficient). Use swap() instead.
  inline JigdoDescVec& operator=(const JigdoDescVec&);
};

inline void swap(JigdoDescVec& x, JigdoDescVec& y) { x.swap(y); }

//======================================================================

JigdoDesc::ImageInfoMD5::ImageInfoMD5(uint64 s, const MD5& m, size_t b)
  : sizeVal(s), md5Val(m), blockLengthVal(b) { }
JigdoDesc::ImageInfoMD5::ImageInfoMD5(uint64 s, const MD5Sum& m, size_t b)
  : sizeVal(s), md5Val(m), blockLengthVal(b) { }
JigdoDesc::ImageInfoSHA256::ImageInfoSHA256(uint64 s, const SHA256& m, size_t b)
  : sizeVal(s), sha256Val(m), blockLengthVal(b) { }
JigdoDesc::ImageInfoSHA256::ImageInfoSHA256(uint64 s, const SHA256Sum& m, size_t b)
  : sizeVal(s), sha256Val(m), blockLengthVal(b) { }

JigdoDesc::MatchedFileMD5::MatchedFileMD5(uint64 o, uint64 s, const RsyncSum64& r,
                                    const MD5& m)
  : offsetVal(o), sizeVal(s), rsyncVal(r), md5Val(m) { }
JigdoDesc::MatchedFileSHA256::MatchedFileSHA256(uint64 o, uint64 s, const RsyncSum64& r,
                                    const SHA256& m)
  : offsetVal(o), sizeVal(s), rsyncVal(r), sha256Val(m) { }
JigdoDesc::MatchedFileMD5::MatchedFileMD5(uint64 o, uint64 s, const RsyncSum64& r,
                                    const MD5Sum& m)
  : offsetVal(o), sizeVal(s), rsyncVal(r), md5Val(m) { }
JigdoDesc::MatchedFileSHA256::MatchedFileSHA256(uint64 o, uint64 s, const RsyncSum64& r,
                                    const SHA256Sum& m)
  : offsetVal(o), sizeVal(s), rsyncVal(r), sha256Val(m) { }

//________________________________________

bool JigdoDesc::ImageInfoMD5::operator==(const JigdoDesc& x) const {
  const ImageInfoMD5* i = dynamic_cast<const ImageInfoMD5*>(&x);
  if (i == 0) return false;
  else return size() == i->size() && md5() == i->md5();
}

bool JigdoDesc::ImageInfoSHA256::operator==(const JigdoDesc& x) const {
  const ImageInfoSHA256* i = dynamic_cast<const ImageInfoSHA256*>(&x);
  if (i == 0) return false;
  else return size() == i->size() && sha256() == i->sha256();
}

bool JigdoDesc::UnmatchedData::operator==(const JigdoDesc& x) const {
  const UnmatchedData* u = dynamic_cast<const UnmatchedData*>(&x);
  if (u == 0) return false;
  else return size() == u->size();
}

bool JigdoDesc::MatchedFileMD5::operator==(const JigdoDesc& x) const {
  const MatchedFileMD5* m = dynamic_cast<const MatchedFileMD5*>(&x);
  if (m == 0) return false;
  else return offset() == m->offset() && size() == m->size()
              && md5() == m->md5();
}

bool JigdoDesc::MatchedFileSHA256::operator==(const JigdoDesc& x) const {
  const MatchedFileSHA256* m = dynamic_cast<const MatchedFileSHA256*>(&x);
  if (m == 0) return false;
  else return offset() == m->offset() && size() == m->size()
              && sha256() == m->sha256();
}

bool JigdoDesc::WrittenFileMD5::operator==(const JigdoDesc& x) const {
  // NB MatchedFileMD5 and WrittenFileMD5 considered equal!
  const MatchedFileMD5* m = dynamic_cast<const MatchedFileMD5*>(&x);
  if (m == 0) return false;
  else return offset() == m->offset() && size() == m->size()
              && md5() == m->md5();
}

bool JigdoDesc::WrittenFileSHA256::operator==(const JigdoDesc& x) const {
  // NB MatchedFileSHA256 and WrittenFileSHA256 considered equal!
  const MatchedFileSHA256* m = dynamic_cast<const MatchedFileSHA256*>(&x);
  if (m == 0) return false;
  else return offset() == m->offset() && size() == m->size()
              && sha256() == m->sha256();
}
//________________________________________

inline bistream& operator>>(bistream& s, JigdoDescVec& v) {
  return v.get(s);
}

inline bostream& operator<<(bostream& s, JigdoDescVec& v) {
  return v.put(s);
}

JigdoDescVec::~JigdoDescVec() {
  for (iterator i = begin(), e = end(); i != e; ++i) delete *i;
}
//________________________________________

template<class Iterator>
Iterator JigdoDesc::ImageInfoMD5::serialize(Iterator i) const {
  i = serialize1(IMAGE_INFO_MD5, i);
  i = serialize6(size(), i);
  i = ::serialize(md5(), i);
  i = serialize4(blockLength(), i);
  return i;
}
size_t JigdoDesc::ImageInfoMD5::serialSizeOf() const { return 1 + 6 + 16 + 4; }

template<class Iterator>
Iterator JigdoDesc::ImageInfoSHA256::serialize(Iterator i) const {
  i = serialize1(IMAGE_INFO_SHA256, i);
  i = serialize6(size(), i);
  i = ::serialize(sha256(), i);
  i = serialize4(blockLength(), i);
  return i;
}
size_t JigdoDesc::ImageInfoSHA256::serialSizeOf() const { return 1 + 6 + 32 + 4; }

template<class Iterator>
Iterator JigdoDesc::UnmatchedData::serialize(Iterator i) const {
  i = serialize1(UNMATCHED_DATA, i);
  i = serialize6(size(), i);
  return i;
}
size_t JigdoDesc::UnmatchedData::serialSizeOf() const { return 1 + 6; }

template<class Iterator>
Iterator JigdoDesc::MatchedFileMD5::serialize(Iterator i) const {
  i = serialize1(MATCHED_FILE_MD5, i);
  i = serialize6(size(), i);
  i = ::serialize(rsync(), i);
  i = ::serialize(md5(), i);
  return i;
}
size_t JigdoDesc::MatchedFileMD5::serialSizeOf() const { return 1 + 6 + 8 + 16;}

template<class Iterator>
Iterator JigdoDesc::MatchedFileSHA256::serialize(Iterator i) const {
  i = serialize1(MATCHED_FILE_SHA256, i);
  i = serialize6(size(), i);
  i = ::serialize(rsync(), i);
  i = ::serialize(sha256(), i);
  return i;
}
size_t JigdoDesc::MatchedFileSHA256::serialSizeOf() const { return 1 + 6 + 8 + 32;}

template<class Iterator>
Iterator JigdoDesc::WrittenFileMD5::serialize(Iterator i) const {
  i = serialize1(WRITTEN_FILE_MD5, i);
  i = serialize6(size(), i);
  i = ::serialize(rsync(), i);
  i = ::serialize(md5(), i);
  return i;
}
size_t JigdoDesc::WrittenFileMD5::serialSizeOf() const { return 1 + 6 + 8 + 16;}

template<class Iterator>
Iterator JigdoDesc::WrittenFileSHA256::serialize(Iterator i) const {
  i = serialize1(WRITTEN_FILE_SHA256, i);
  i = serialize6(size(), i);
  i = ::serialize(rsync(), i);
  i = ::serialize(sha256(), i);
  return i;
}
size_t JigdoDesc::WrittenFileSHA256::serialSizeOf() const { return 1 + 6 + 8 + 32;}


#endif
