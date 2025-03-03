/* $Id: datasource.hh,v 1.7 2005/04/09 23:09:52 atterer Exp $ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/�|  Richard Atterer     |  atterer.org
  � '` �

  Copyright (C) 2021 Steve McIntyre <steve@einval.com>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

*//** @file

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

    /** Called by the job when it is deleted. If the IO object considers
        itself owned by its job, it can delete itself. */
    virtual void job_deleted() = 0;

    /** Called when the job has successfully completed its task. */
    virtual void job_succeeded() = 0;

    /** Called when the job fails. The only remaining action after getting
        this is to delete the job object. */
    virtual void job_failed(const string& message) = 0;

    /** Informational message. */
    virtual void job_message(const string& message) = 0;

    /** Called as soon as the size of the downloaded data is known. May not
        be called at all if the size is unknown. Problem with libwww: Returns
        size as long int - 2 GB size limit! */
    virtual void dataSource_dataSize(uint64 n) = 0;

    /** Called during download whenever data arrives, with the data that just
        arrived. You can write the data to a file, copy it away etc.
        currentSize is the offset into the downloaded data (including the
        "size" new bytes) - useful for "x% done" messages. */
    virtual void dataSource_data(const Ubyte* data, unsigned size,
                                 uint64 currentSize) = 0;
  };
  //____________________

  IOSource<IO> io;

  DataSource() : io() { };
  virtual ~DataSource() { }

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

#endif
