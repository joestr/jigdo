/* $Id: mktemplate.hh,v 1.29 2002/02/21 22:29:11 richard Exp $ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2000-2002 Richard Atterer
  | \/¯|  <richard@atterer.net>
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Create location list (.jigdo) and image template (.template)

*/

#ifndef MKTEMPLATE_HH
#define MKTEMPLATE_HH

#include <iostream>
#include <string>
#include <set>
#include <vector>
#include <iosfwd>
namespace std { }
using namespace std;

#include <bstream.hh>
#include <debug.hh>
#include <md5sum.hh>
#include <rsyncsum.hh>
#include <scan.fh>
#include <zstream.fh>
//______________________________________________________________________

/** Create location list (jigdo) and image template (template) from
    one big file and a list of files. The template file contains a
    compressed version of the big file, excluding the data of any of
    the other files that are contained somewhere in the big file.
    Instead of their data, the image template file just lists their
    checksums. */
class MkTemplate {
public:
  class ProgressReporter;

  /** A create operation with no files known to it yet.
      @param jcache Cache for the files (will not be deleted in dtor)
      @param imageStream The large image file
      @param jigdoStream Stream for outputting location list
      @param templateStream Stream for outputting binary image template
      @param pr Function object which is called at regular intervals
      during run() to inform about files scanned, nr of bytes scanned,
      matches found etc.
      @param minLen Minimum length of a part for it to be "worth it"
      to exclude it from image template, i.e. only include it by MD5
      reference. Any value smaller than 4k is silently set to 4k.
      @param readAmnt Number of bytes that are read at a time with one
      read() call by the operation before the data is processed.
      Should not be too large because the OS copes best when small
      bits of I/O are interleaved with small bits of CPU work. In
      practice, the default seems to work well.
      @param addImage Add a [Image] section to the output .jigdo.
      @param addServers Add a [Servers] section to the output .jigdo. */
  MkTemplate(JigdoCache* jcache, bistream* imageStream, ostream* jigdoStream,
             bostream* templateStream, ProgressReporter& pr = noReport,
             int zipQuality = 9, size_t readAmnt = 128U*1024,
             bool addImage = true, bool addServers = true);
  ~MkTemplate() { }

  /** First scan through all the individual files, creating checksums,
      then read image file and find matches. Write .template and .jigdo
      files.
      @param imageLeafName Name for the image, which should be a
      relative path name. This does not need to be similar to the
      filename that the image has now - it is *only* used when
      creating the .jigdo file, nowhere else. It is the name that will
      be used when someone reassembles the image.
      @return returns FAILURE if any open/read/write/close failed,
      unless it was that of a file with reportErrors==false. */
  bool run(const string& imageLeafName = "image");

  /// Default reporter: Only prints error messages to stderr
  static ProgressReporter noReport;

private:
  // Values for codes in the template data's DESC section
  static const byte IMAGE_INFO = 1, UNMATCHED_DATA = 2, MATCHED_FILE = 3;
  static const size_t REPORT_INTERVAL = 256U*1024;
  /* The size of the linked list of MD5 blocks awaiting matching must
     be limited for cases where there are lots of overlapping matches,
     e.g. both image and a file are all zeroes. */
  static const int TODO_MAX = 32;

  // Disallow assignment; op is never defined
  inline MkTemplate& operator=(const MkTemplate&);

  // Various helper classes and functions for run()
  class Desc;
  inline bool MkTemplate::scanFiles(size_t blockLength, uint32 blockMask,
    size_t md5BlockLength);
  inline bool scanImage(byte* buf, size_t bufferLength, size_t blockLength,
    uint32 blockMask, size_t md5BlockLength, set<FilePart*>& matched);
  struct PartialMatch;
  static inline void insertInTodo(PartialMatch*& todo, PartialMatch* x);
  void checkRsyncSumMatch2(const size_t blockLen, const size_t back,
    const uint64 off, const size_t md5BlockLength, PartialMatch*& todo,
    PartialMatch*& todoFree, uint64& nextEvent, const uint64 unmatchedStart,
    FilePart* file);
  inline void checkRsyncSumMatch(const RsyncSum64& sum,
    const uint32& bitMask, const size_t blockLen, const size_t back,
    const uint64 off, const size_t md5BlockLength, PartialMatch*& todo,
    PartialMatch*& todoFree, uint64& nextEvent, const uint64 unmatchedStart);
  inline bool checkMD5Match(const uint64 off, byte* const buf,
    const size_t bufferLength, const size_t data, PartialMatch*& todo,
    PartialMatch*& todoFree, const size_t md5BlockLength, uint64& nextEvent,
    Zobstream& zip, size_t stillBuffered, Desc& desc, uint64& unmatchedStart,
    set<FilePart*>& matched);
  inline bool unmatchedAtEnd(const uint64 off, byte* const buf,
    const size_t bufferLength, const size_t data, PartialMatch*& todo,
    Zobstream& zip, Desc& desc, uint64& unmatchedStart);
  bool rereadUnmatched(const PartialMatch* x, uint64 count, Zobstream& zip);
  inline bool writeJigdo(set<FilePart*>& matched,
    const string& imageLeafName);
# if DEBUG
  void debugRangeInfo(uint64, uint64, const char*,
                      const PartialMatch* = 0);
# else
  inline void debugRangeInfo(uint64, uint64, const char*,
                             const PartialMatch* = 0);
# endif

  uint64 fileSizeTotal; // Accumulated lengths of all the files
  size_t fileCount; // Total number of files added

  // Look up list of FileParts by hash of RsyncSum
  typedef vector<vector<FilePart*> > FileVec;
  FileVec block;

  // Nr of bytes to read in one go, as specified by caller of MkTemplate()
  size_t readAmount;

  JigdoCache* cache;
  bistream* image;
  ostream* jigdo;
  bostream* templ;

  int zipQual; // 0..9, passed to zlib
  ProgressReporter& reporter;

  // true => add a [Image/Servers] section to the output .jigdo file
  bool addImageSection;
  bool addServersSection;

  // For debugging of the template creation
  uint64 oldAreaEnd;
};
//______________________________________________________________________

/** Class allowing MkTemplate to convey information back to the
    creator of a MkTemplate object. The default versions of the
    methods do nothing at all (except for error(), which prints the
    error to cerr) - you need to supply an object of a derived class
    to MkTemplate() to get called back. */
class MkTemplate::ProgressReporter {
public:
  virtual ~ProgressReporter() { }
  /// General-purpose error reporting
  virtual void error(const string& message);
  /** Called during second pass, when the image file is scanned.
      @param offset Offset in image file
      @param total Total size of image file, or 0 if unknown (i.e.
      fed to stdin */
  virtual void scanningImage(uint64 offset);
  /// Error while reading the image file - aborting immediately
//   virtual void abortingScan();
  /** Called during second pass, when MkTemplate has found an area in
      the image whose MD5Sum matches that of a file
      @param file The file that matches
      @param offInImage Offset of the match from start of image */
  virtual void matchFound(const FilePart* file, uint64 offInImage);
  /** The MkTemplate operation has finished.
      @param imageSize Length of the image file */
  virtual void finished(uint64 imageSize);
};
//______________________________________________________________________

#  if !DEBUG
void MkTemplate::debugRangeInfo(uint64 start, uint64 end, const char*,
                                const PartialMatch*) {
  Assert(oldAreaEnd == start);
  oldAreaEnd = end;
}
#  endif

#endif
