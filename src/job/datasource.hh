/* $Id: datasource.hh,v 1.4 2003/08/28 23:21:00 atterer Exp $ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/�|  Richard Atterer     |  atterer.net
  � '` �
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Interface for objects returning data from the network or from disk

*/

#ifndef DATASOURCE_HH
#define DATASOURCE_HH

#include <config.h>

#include <job.hh>
#include <makeimagedl.fh>
#include <nocopy.hh>
#include <progress.fh>
//______________________________________________________________________

namespace Job {
  class DataSource;
}

/** Interface for objects returning data from the network or from disk.

    Implemented by SingleUrl and CachedUrl. MakeImageDl::dataSourceFor() is
    the function which examines the local jigdo download's temporary
    directory and creates a SingleUrl/CachedUrl as appropriate before putting
    it inside a MakeImageDl::Child. */
class Job::DataSource : NoCopy {
public:

  /** User interaction for DataSource.

      Instances of derived classes are attached to any DataSources that the
      MakeImageDl creates. */
  class IO : public Job::IO {
  public:

    // Not overriding here:
    // virtual IO* job_removeIo(IO* rmIo);

    /** Called by the job when it is deleted or when a different IO object is
        registered with it. If the IO object considers itself owned by its
        job, it can delete itself. */
    virtual void job_deleted() = 0;

    /** Called when the job has successfully completed its task. */
    virtual void job_succeeded() = 0;

    /** Called when the job fails. The only remaining action after getting
        this is to delete the job object. */
    virtual void job_failed(string* message) = 0;

    /** Informational message. */
    virtual void job_message(string* message) = 0;

    /** Called as soon as the size of the downloaded data is known. May not
        be called at all if the size is unknown. Problem with libwww: Returns
        size as long int - 2 GB size limit! */
    virtual void dataSource_dataSize(uint64 n) = 0;

    /** Called during download whenever data arrives, with the data that just
        arrived. You can write the data to a file, copy it away etc.
        currentSize is the offset into the downloaded data (including the
        "size" new bytes) - useful for "x% done" messages. */
    virtual void dataSource_data(const byte* data, unsigned size,
                                 uint64 currentSize) = 0;
  };
  //____________________

  IOPtr<IO> io;

  inline DataSource(IO* ioPtr);
  virtual ~DataSource();

  /** Start delivering data. */
  virtual void run() = 0;

  /** Is the data stream currently paused? */
  virtual bool paused() const = 0;
  /** Pause the data stream. */
  virtual void pause() = 0;
  /** Continue after pause(). */
  virtual void cont() = 0;

  /** Return the internal progress object. */
  virtual const Progress* progress() const = 0;

  /** Return the URL used to download the data, or its filename on disc */
  virtual const string& location() const = 0;
};
//______________________________________________________________________

Job::DataSource::DataSource(IO* ioPtr) : io(ioPtr) { }

#endif
