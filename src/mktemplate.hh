/* $Id: mktemplate.hh,v 1.10 2006/05/14 18:23:31 atterer Exp $ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2000-2002  |  richard@
  | \/�|  Richard Atterer          |  atterer.org
  � '` �

  Copyright (C) 2016-2021 Steve McIntyre <steve@einval.com>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

*//** @file

  Create location list (.jigdo) and image template (.template)

*/

#ifndef MKTEMPLATE_HH
#define MKTEMPLATE_HH

#ifndef INLINE
#  ifdef NOINLINE
#    define INLINE
#    else
#    define INLINE inline
#  endif
#endif

#include <config.h>

#include <iostream>
#include <string>
#include <set>
#include <vector>
#include <iosfwd>

#include <bstream.hh>
#include <compat.hh>
#include <configfile.hh>
#include <debug.hh>
#include <jigdoconfig.fh>
#include <log.hh>
#include <md5sum.hh>
#include <sha256sum.hh>
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
  /* Choice of checksum algorithm to use, i.e. choice of ImageInfo*
   * and MatchedFile* Desc entries */
  enum ChecksumType {
    CHECK_MD5 = 1,
    CHECK_SHA256 = 2,
  };

  class ProgressReporter;

  /** A create operation with no files known to it yet.
      @param jcache Cache for the files (will not be deleted in dtor)
      @param imageStream The large image file
      @param jigdoInfo Where to output .jigdo data to
      @param templateStream Stream for outputting binary image template
      @param pr Function object which is called at regular intervals
      during run() to inform about files scanned, nr of bytes scanned,
      matches found etc.
      @param zipQuality 0 (fast) to 9 (smallest output)
      @param readAmnt Number of bytes that are read at a time with one
      read() call by the operation before the data is processed.
      Should not be too large because the OS copes best when small
      bits of I/O are interleaved with small bits of CPU work. In
      practice, the default seems to work well.
      @param addImage Add a [Image] section to the output .jigdo.
      @param addServers Add a [Servers] section to the output .jigdo.
      @param useBzip2 false=>gzip, true=>bzip2 */
  MkTemplate(JigdoCache* jcache, bistream* imageStream,
             JigdoConfig* jigdoInfo, bostream* templateStream,
             ProgressReporter& pr = noReport, int zipQuality = 9,
             size_t readAmnt = 128U*1024, bool addImage = true,
             bool addServers = true, bool useBzip2 = false,
	     int checksumChoice = CHECK_MD5);
  inline ~MkTemplate();

  /** Set command(s) to be executed when a file matches. */
  inline void setMatchExec(const string& me);

  /** Set and get whether to skip smaller matches if a larger match could be
      possible (with the risk to skip both). True <=> prefer small matches;
      false <=> skip small, prefer large */
  inline void setGreedyMatching(bool x) { greedyMatching = x; }
  inline bool getGreedyMatching() const { return greedyMatching; }

  /** First scan through all the individual files, creating checksums,
      then read image file and find matches. Write .template and .jigdo
      files.
      @param imageLeafName Name for the image, which should be a
      relative path name. This does not need to be similar to the
      filename that the image has now - it is *only* used when
      creating the .jigdo file, nowhere else. It is the name that will
      be used when someone reassembles the image.
      @param templLeafName Name to write to jigdo file as template
      URL. Can be a full http/ftp URL, or a relative URL.
      @return returns FAILURE if any open/read/write/close failed,
      unless it was that of a file with reportErrors==false. */
  bool run(const string& imageLeafName = "image",
           const string& templLeafName = "template");

  /** Default reporter: Only prints error messages to stderr */
  static ProgressReporter noReport;

private:
  // Values for codes in the template data's DESC section
  static const Ubyte IMAGE_INFO = 1, UNMATCHED_DATA = 2, MATCHED_FILE = 3;
  static const size_t REPORT_INTERVAL = 256U*1024;
  /* If the queue of matches runs full, we enter a special mode where new
     matches are only added to the queue if their start offset is a multiple
     of this assumed sector size. This is a heuristics - matches are more
     likely to start at "even" offsets. Because the actual sector size of the
     input image is not known, we'll prefer
     SECTOR_LENGTH-aligned matches to matches at odd offsets,
     2*SECTOR_LENGTH-aligned ones to SECTOR_LENGTH-aligned ones,
     4*SECTOR_LENGTH-aligned ones to 2*SECTOR_LENGTH-aligned ones etc.
     Initial value for sectorLength. */
  static const unsigned INITIAL_SECTOR_LENGTH = 512;
  static const unsigned MAX_SECTOR_LENGTH = 65536;

  /* debug(...) may be defined as a CPP macro. Luckily, that won't affect
     this occurance of the word. */
  static Logger debug;

  // Disallow assignment; op is never defined
  inline MkTemplate& operator=(const MkTemplate&);

  // Various helper classes and functions for run()
  class Desc;
  class PartialMatch;
  class PartialMatchQueue;
  friend class PartialMatchQueue;
  void prepareJigdo(const int major, const int minor);
  void finalizeJigdo(const string& imageLeafName,
    const string& templLeafName, const MD5Sum& templMd5Sum,
    const SHA256Sum& templSHA256Sum, int checksumChoice);
  INLINE bool scanFiles(size_t blockLength, uint32 blockMask,
    size_t csumBlockLength);
  INLINE bool scanImage(Ubyte* buf, size_t bufferLength, size_t blockLength,
    uint32 blockMask, size_t csumBlockLength, MD5Sum&, SHA256Sum&);
  static INLINE void insertInTodo(PartialMatchQueue& matches,
    PartialMatch* x);
  void checkRsyncSumMatch2(const size_t blockLen, const size_t back,
    const size_t csumBlockLength, uint64& nextEvent, FilePart* file);
  INLINE void checkRsyncSumMatch(const RsyncSum64& sum,
    const uint32& bitMask, const size_t blockLen, const size_t back,
    const size_t csumBlockLength, uint64& nextEvent);
  INLINE bool checkChecksumMatch(Ubyte* const buf,
    const size_t bufferLength, const size_t data,
    const size_t csumBlockLength, uint64& nextEvent,
    const size_t stillBuffered, Desc& desc);
  INLINE bool checkMatch_mismatch(const size_t stillBuffered,
    PartialMatch* x, Desc& desc);
  INLINE bool unmatchedAtEnd(Ubyte* const buf, const size_t bufferLength,
    const size_t data, Desc& desc);
  bool rereadUnmatched(FilePart* file, uint64 count);
  INLINE void scanImage_mainLoop_fastForward(uint64 nextEvent,
    RsyncSum64* rsum, Ubyte* buf, size_t* data, size_t* n, size_t* rsumBack,
    size_t bufferLength, size_t blockLength, uint32 blockMask,
    size_t csumBlockLength);
  INLINE bool matchExecCommands(PartialMatch* x);

  inline void debugRangeInfo(uint64 start, uint64 end, const char* msg,
                             const PartialMatch* x = 0);
  void printRangeInfo(uint64 start, uint64 end, const char* msg,
                      const PartialMatch* x = 0);
  void debugRangeFailed();

  uint64 fileSizeTotal; // Accumulated lengths of all the files
  size_t fileCount; // Total number of files added

  // Look up list of FileParts by hash of RsyncSum
  typedef vector<vector<FilePart*> > FileVec;
  FileVec block;

  // Nr of bytes to read in one go, as specified by caller of MkTemplate()
  size_t readAmount;

  /* The area delimited by unmatchedStart (incl) and off (excl) has "not been
     dealt with", either by writing it to zip, or by a match with the first
     MD5 block of an input file. Once a partial match of a file has been
     detected, unmatchedStart "gets stuck" at the start offset of this file
     within the image. */
  uint64 off; // Current absolute offset in image
  uint64 unmatchedStart;

  bool greedyMatching;

  JigdoCache* cache;
  bistream* image;
  bostream* templ;
  Zobstream* zip; // Compressing stream for template data output

  int zipQual; // 0..9, passed to zlib
  ProgressReporter& reporter;
  PartialMatchQueue* matches; // queue of partially matched files
  unsigned sectorLength;
  //____________________

  JigdoConfig* jigdo;
  struct PartLine;
  struct PartIndex;
  set<PartLine> jigdoParts; // lines of [Parts] section
  vector<FilePart*> matchedParts; // New parts to add to [Parts] at end
  // true => add a [Image/Servers] section to the output .jigdo file
  bool addImageSection;
  bool addServersSection;
  bool useBzLib;
  int useChecksum;
  string matchExec;
  //____________________

  // For debugging of the template creation
  uint64 oldAreaEnd;
};

#include <partialmatch.hh>
//______________________________________________________________________

/** Class allowing MkTemplate to convey information back to the
    creator of a MkTemplate object. The default versions of the
    methods do nothing at all (except for error(), which prints the
    error to cerr) - you need to supply an object of a derived class
    to MkTemplate() to get called back. */
class MkTemplate::ProgressReporter {
public:
  virtual ~ProgressReporter() { }
  /** General-purpose error reporting */
  virtual void error(const string& message);
  /** Called during second pass, when the image file is scanned.
      @param offset Offset in image file */
  virtual void scanningImage(uint64 offset);
  // Error while reading the image file - aborting immediately
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

/** Line content with whitespace and '=' removed and left/right side
    swapped, i.e. " xyz= foo" becomes "fooxyz". */
struct MkTemplate::PartLine {
  string text;
  size_t split; // Offset of first char after "foo"
  bool operator<(const PartLine& x) const {
    int cmp = text.compare(x.text);
    return cmp < 0 || (cmp == 0 && split < x.split);
  }
};

MkTemplate::~MkTemplate() { }

void MkTemplate::setMatchExec(const string& me) { matchExec = me; }

void MkTemplate::debugRangeInfo(uint64 start, uint64 end, const char* msg,
                                const PartialMatch* x) {
  printRangeInfo(start, end, msg, x);
  if (oldAreaEnd != start) debugRangeFailed();
  oldAreaEnd = end;
}

#endif
