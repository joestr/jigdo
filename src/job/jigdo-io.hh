/* $Id: jigdo-io.hh,v 1.16 2005/04/09 23:09:52 atterer Exp $ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/�|  Richard Atterer     |  atterer.org
  � '` �

  Copyright (C) 2021 Steve McIntyre <steve@einval.com>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

*//** @file

  IO object for .jigdo downloads; download, gunzip, interpret

*/

#ifndef JIGDO_IO_HH
#define JIGDO_IO_HH

#include <config.h>
#include <vector>
#include <string>

#include <datasource.hh>
#include <gunzip.hh>
#include <jigdo-io.fh>
#include <makeimagedl.hh>
#include <md5sum.fh>
#include <nocopy.hh>
#include <status.hh>
//______________________________________________________________________

namespace Job {
  class JigdoIO;
  struct JigdoIOTest;
}

/** IO object for .jigdo downloads; download, gunzip, interpret */
class Job::JigdoIO : NoCopy, public Job::DataSource::IO, Gunzip::IO {
public:

  /** Create a new JigdoIO which is owned by m, gets data from download (will
      register itself with download's IOPtr) and passes it on to childIo.
      @param c Object which owns us (it is the MakeImageDl's child, but our
      master)
      @param url URL of the .jigdo file */
  JigdoIO(MakeImageDl::Child* c, const string& url/* IOPtr,
          DataSource::IO* frontendIo*/);
  ~JigdoIO();

  inline MakeImageDl* master() const;
  inline DataSource* source() const;

private:
  friend struct Job::JigdoIOTest;

  /* Create object for an [Include]d file */
  JigdoIO(MakeImageDl::Child* c, const string& url,
          /*DataSource::IO* frontendIo,*/ JigdoIO* parentJigdo,
          int inclLine);

  /** @return Root object of the include tree */
  inline JigdoIO* root();
  inline const JigdoIO* root() const;
  /** @return true iff this object is the root of the include tree. */
  inline bool isRoot() const;
  /** Return the ptr to the image section candidate object; the JigdoIO which
      might or might not contain the first [Image] section. If new data is
      received for that object and that new data contains an [Image], we know
      it's the first [Image]. If all data is recvd without any [Image]
      turning up, we continue walking the include tree depth-first. */
  inline JigdoIO* imgSectCandidate() const;
  /** Set the ptr to the image section candidate object */
  inline void setImgSectCandidate(JigdoIO* c);
  // The methods below are called in various places to find 1st image section
  inline void imgSect_newChild(JigdoIO* child); // Child created after [Incl.
  inline void imgSect_parsed(); // [Image] occurred in current .jigdo data
  /* End of current file without any [Image]. Returns 1 if OK and all
     JigdoIOs finished */
  inline XStatus imgSect_eof();

  // Create error message with URL and line number
  void generateError(const string& msg);
  void generateError(const char* msg);
  // As above, but directly pass on error string, do not add URL/line
  void generateError_plain(const string& err);
  // True after above was called
  inline bool failed() const;
  // Called by gunzip_data(): New .jigdo line ready. Arg is empty on exit.
  void jigdoLine(string* l);
  void include(string* url); // "[Include http://xxx]" found
  void entry(string* label, string* data, unsigned valueOff);
  /* Called at the end of a [Section] (=start of another section or EOF)
     Returns FAILURE if there is an error. */
  Status sectionEnd();

  // Virtual methods from DataSource::IO
  virtual void job_deleted();
  virtual void job_succeeded();
  virtual void job_failed(const string& message);
  virtual void job_message(const string& message);
  virtual void dataSource_dataSize(uint64 n);
  virtual void dataSource_data(const Ubyte* data, unsigned size,
                               uint64 currentSize);

  // Virtual methods from Gunzip::IO
  virtual void gunzip_deleted();
  virtual void gunzip_data(Gunzip*, Ubyte* decompressed, unsigned size);
  virtual void gunzip_needOut(Gunzip*);
  virtual void gunzip_failed(string* message);

  MakeImageDl::Child* childDl;
  string urlVal; // Absolute URL of this .jigdo file

  /* Representation of the tree of [Include] directives. Most of the time,
     the order of data in the .jigdo files is not relevant, with one
     exception: We must interpret the first [Image] section only, and ignore
     all following ones. */
  JigdoIO* parent; // .jigdo file which [Include]d us, or null if top-level
  int includeLine; // If parent!=null, line num of [Include] in parent
  JigdoIO* firstChild; // First file we [Include], or null if none
  JigdoIO* next; // Right sibling, or null if none
  /* For the root object, contains imgSectCandidate, else ptr to root object.
     Don't access directly, use accessor methods. */
  JigdoIO* rootAndImageSectionCandidate;

  int line; // Line number, for errors. 0 if no data yet, -1 if finished
  bool finished() { return line < 0; }
  void setFinished() { line = -1; }
  string section; // Current section name, empty if none yet

  // Info about first image section of this .jigdo, if any
  int imageSectionLine; // 0 if no [Image] found yet
  string imageName;
  string imageInfo, imageShortInfo;
  SmartPtr<PartUrlMapping> templateUrls; // Can contain a list of altern. URLs
  MD5* templateMd5;

  /* Transparent gunzipping of .jigdo file. GUNZIP_BUF_SIZE is also the max
     size a single line in the .jigdo is allowed to have */
  static const unsigned GUNZIP_BUF_SIZE = 16384;
  Gunzip gunzip;
  Ubyte gunzipBuf[GUNZIP_BUF_SIZE];
};
//______________________________________________________________________

Job::JigdoIO* Job::JigdoIO::root() {
  if (isRoot()) return this; else return rootAndImageSectionCandidate;
}
const Job::JigdoIO* Job::JigdoIO::root() const {
  if (isRoot()) return this; else return rootAndImageSectionCandidate;
}
bool Job::JigdoIO::isRoot() const {
  return parent == 0;
}

Job::JigdoIO* Job::JigdoIO::imgSectCandidate() const {
  if (isRoot())
    return rootAndImageSectionCandidate;
  else
    return rootAndImageSectionCandidate->rootAndImageSectionCandidate;
}
void Job::JigdoIO::setImgSectCandidate(JigdoIO* c) {
  if (isRoot())
    rootAndImageSectionCandidate = c;
  else
    rootAndImageSectionCandidate->rootAndImageSectionCandidate = c;
}

Job::MakeImageDl* Job::JigdoIO::master() const { return childDl->master(); }
Job::DataSource*  Job::JigdoIO::source() const { return childDl->source(); }

bool Job::JigdoIO::failed() const {
  return (imageName.length() == 1 && imageName[0] == '\0');
  //return (childFailedId != 0);
}

#endif
