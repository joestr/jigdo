/* $Id: zstream-gz.hh,v 1.5 2005/04/09 23:09:52 atterer Exp $ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2004-2005  |  richard@
  | \/�|  Richard Atterer          |  atterer.org
  � '` �

  Copyright (C) 2016-2021 Steve McIntyre <steve@einval.com>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

*//** @file

  zlib (gzip-style) compression and decompression for zstream

*/

#ifndef ZSTREAM_GZ_HH
#define ZSTREAM_GZ_HH

#include <config.h>

#include <zlib.h>

#include <log.hh>
#include <zstream.hh>
//______________________________________________________________________

struct ZerrorGz : public Zerror {
  ZerrorGz(int s, const string& m) : Zerror(s, m) { }
  int status;
};
//______________________________________________________________________

class ZobstreamGz : public Zobstream {
public:
  inline ZobstreamGz(bostream& s, unsigned chunkLimit,
                     int level = Z_DEFAULT_COMPRESSION, int windowBits = 15,
                     int memLevel = 8, unsigned todoBufSz = 256U,
                     MD5Sum* md = 0, SHA256Sum* sd = 0);
  ~ZobstreamGz() { Assert(memReleased); }

  /** @param s Output stream
      @param chunkLimit Size limit for output data, will buffer this much
      @param level 0 to 9
      @param windowBits zlib param
      @param memLevel zlib param
      @param todoBufSz Size of mini buffer, which holds data sent to
      the stream with single put() calls or << statements */
  void open(bostream& s, unsigned chunkLimit, int level =Z_DEFAULT_COMPRESSION,
            int windowBits = 15, int memLevel = 8, unsigned todoBufSz = 256U);

protected:
  virtual unsigned partId();
  virtual void deflateEnd();
  virtual void deflateReset();
  virtual unsigned totalOut() const { return (unsigned)z.total_out; }
  virtual unsigned totalIn() const { return (unsigned)z.total_in; }
  virtual unsigned availOut() const { return z.avail_out; }
  virtual unsigned availIn() const { return z.avail_in; }
  virtual Ubyte* nextOut() const { return z.next_out; }
  virtual Ubyte* nextIn() const { return z.next_in; }
  virtual void setTotalOut(unsigned n) { z.total_out = n; }
  virtual void setTotalIn(unsigned n) { z.total_in = n; }
  virtual void setAvailOut(unsigned n) { z.avail_out = n; }
  virtual void setAvailIn(unsigned n) { z.avail_in = n; }
  virtual void setNextOut(Ubyte* n) { z.next_out = n; }
  virtual void setNextIn(Ubyte* n) { z.next_in = n; }
  virtual void zip2(Ubyte* start, unsigned len, bool finish = false);

private:
  // Throw a Zerror exception, or bad_alloc() for status==Z_MEM_ERROR
//   inline void throwZerror(int status, const char* zmsg);

  z_stream z;
  // To keep track in the dtor whether deflateEnd() has been called
  bool memReleased;
};
//______________________________________________________________________

class ZibstreamGz : public Zibstream::Impl {
public:

  class ZibstreamGzError : public Zerror {
  public:
    ZibstreamGzError(int s, const string& m) : Zerror(s, m) { }
  };

  ZibstreamGz() : status(0), memReleased(true) { }
  ~ZibstreamGz() { Assert(memReleased); }

  virtual unsigned totalOut() const { return (unsigned)z.total_out; }
  virtual unsigned totalIn() const { return (unsigned)z.total_in; }
  virtual unsigned availOut() const { return z.avail_out; }
  virtual unsigned availIn() const { return z.avail_in; }
  virtual Ubyte* nextOut() const { return z.next_out; }
  virtual Ubyte* nextIn() const { return z.next_in; }
  virtual void setTotalOut(unsigned n) { z.total_out = n; }
  virtual void setTotalIn(unsigned n) { z.total_in = n; }
  virtual void setAvailIn(unsigned n) { z.avail_in = n; }
  virtual void setNextIn(Ubyte* n) { z.next_in = n; }

  virtual void init() {
    //memset(&z, 0, sizeof(z));
    z.zalloc = (alloc_func)0;
    z.zfree = (free_func)0;
    z.opaque = 0;
    status = inflateInit(&z);
    if (ok()) memReleased = false;
  }
  virtual void end() { status = inflateEnd(&z); memReleased = true; }
  virtual void reset() { status = inflateReset(&z); }

  virtual void inflate(Ubyte** nextOut, unsigned* availOut) {
    z.next_out = *nextOut; z.avail_out = *availOut;
    status = ::inflate(&z, Z_NO_FLUSH);
    *nextOut = z.next_out; *availOut = z.avail_out;
  }
  virtual bool streamEnd() const { return status == Z_STREAM_END; }
  virtual bool ok() const { return status == Z_OK; }

  virtual void throwError() const { throw ZibstreamGzError(status, z.msg); }
private:
  int status;
  z_stream z;
  // To keep track in the dtor whether deflateEnd() has been called
  bool memReleased;
};
//======================================================================

ZobstreamGz::ZobstreamGz(bostream& s, unsigned chunkLimit, int level,
                         int windowBits, int memLevel, unsigned todoBufSz,
                         MD5Sum* md, SHA256Sum* sd)
	: Zobstream(md, sd), memReleased(true) {
  z.zalloc = (alloc_func)0;
  z.zfree = (free_func)0;
  z.opaque = 0;
  open(s, chunkLimit, level, windowBits, memLevel, todoBufSz);
}

#endif
