/* $Id: zstream.hh,v 1.10 2005/07/02 22:05:04 atterer Exp $ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2005  |  richard@
  | \/�|  Richard Atterer          |  atterer.org
  � '` �

  Copyright (C) 2016-2021 Steve McIntyre <steve@einval.com>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

*//** @file

  Zlib/bzlib2 compression layer which integrates with C++ streams. When
  deflating, chops up data into DATA chunks of approximately zippedBufSz (see
  ctor below and ../doc/TechDetails.txt).

  Maybe this should use streambuf, but 1) I don't really understand
  streambuf, and 2) the C++ library that comes with GCC 2.95 probably doesn't
  fully support it (not sure tho).

*/

#ifndef ZSTREAM_HH
#define ZSTREAM_HH

#include <config.h>

#include <iostream>

#include <bstream.hh>
#include <config.h>
#include <debug.hh>
#include <md5sum.fh>
#include <sha256sum.fh>
#include <zstream.fh>
//______________________________________________________________________

/** Error messages returned by the zlib library. Note that bad_alloc()
    is thrown if zlib returns Z_MEM_ERROR. Base class for ZerrorGz. */
struct Zerror : Error {
  Zerror(int s, const string& m) : Error(m), status(s) { }
  int status;
};

/** Output stream which compresses the data sent to it before writing
    it to its final destination. The b is for buffered - this class
    will buffer *all* generated output in memory until the bytes
    written to the underlying stream (i.e. the compressed data) exceed
    chunkLimit - once this happens, the zlib data is flushed and the
    resultant compressed chunk written out with a header (cf
    ../doc/TechDetails.txt for format). This is jigdo-specific.

    Additional features mainly useful for jigdo: If an MD5Sum or
    SHA256Sum object is passed to Zobstream(), any data written to the
    output stream is also passed to the object. */
class Zobstream {
public:

  inline explicit Zobstream(MD5Sum* md = 0, SHA256Sum* sd = 0);
  /** Calls close(), which might throw a Zerror exception! Call
      close() before destroying the object to avoid this. */
  virtual ~Zobstream() { close(); delete zipBuf; Assert(todoBuf == 0); }
  bool is_open() const { return stream != 0; }
  /** Forces any remaining data to be compressed and written out */
  void close();

  /** Get reference to underlying ostream */
  bostream& getStream() { return *stream; }

  /** Output 1 character */
  inline Zobstream& put(unsigned char x);
  inline Zobstream& put(signed char x);
  /** Output the low 8 bits of an integer */
  inline Zobstream& put(int x);
  inline Zobstream& put(char x);
  /** Output 32 bit integer in little-endian order */
  Zobstream& put(uint32 x);
  /* Output n characters */
//   inline Zobstream& write(const char* x, size_t n);
//   inline Zobstream& write(const signed char* x, size_t n);
//   Zobstream& write(const unsigned char* x, size_t n);
//   inline Zobstream& write(const void* x, size_t n);
  inline Zobstream& write(const Ubyte* x, unsigned n);

protected:
  static const unsigned ZIPDATA_SIZE = 64*1024;

  // Child classes must call this when their open() is called
  inline void open(bostream& s, unsigned chunkLimit, unsigned todoBufSz);
  unsigned chunkLim() const { return chunkLimVal; }
  // Write data in zipBuf
  void writeZipped(unsigned partId);

  virtual void deflateEnd() = 0; // May throw Zerror
  virtual void deflateReset() = 0; // May throw Zerror

  virtual unsigned totalOut() const = 0;
  virtual unsigned totalIn() const = 0;
  virtual unsigned availOut() const = 0;
  virtual unsigned availIn() const = 0;
  virtual Ubyte* nextOut() const = 0;
  virtual Ubyte* nextIn() const = 0;
  virtual void setTotalOut(unsigned n) = 0;
  virtual void setTotalIn(unsigned n) = 0;
  virtual void setAvailOut(unsigned n) = 0;
  virtual void setAvailIn(unsigned n) = 0;
  virtual void setNextOut(Ubyte* n) = 0;
  virtual void setNextIn(Ubyte* n) = 0;

  virtual void zip2(Ubyte* start, unsigned len, bool finish) = 0;

  /* Compressed data is stored in a linked list of ZipData objects.
     During the Zobstream object's lifetime, the list is only ever
     added to, never shortened. */
  struct ZipData {
    ZipData() : next(0) { }
    ~ZipData() { delete next; }
    ZipData* next;
    Ubyte data[ZIPDATA_SIZE];
  };
  ZipData* zipBuf; // Start of linked list
  ZipData* zipBufLast; // Last link

private:
  static const unsigned MIN_TODOBUF_SIZE = 256;

//   // Throw a Zerror exception, or bad_alloc() for status==Z_MEM_ERROR
//   inline void throwZerror(int status, const char* zmsg);
  // Pipe contents of todoBuf through zlib into zipBuf
  //  void zip(Ubyte* start, unsigned len, int flush = Z_NO_FLUSH);
  inline void zip(Ubyte* start, unsigned len, bool finish = false);

  //z_stream z;
  Ubyte* todoBuf; // Allocated during open(), deallocated during close()
  unsigned todoBufSize; // Size of todoBuf
  unsigned todoCount; // Offset of 1st unused Ubyte in todoBuf

  bostream* stream;

  unsigned chunkLimVal;

  MD5Sum* md5sum;
  SHA256Sum* sha256sum;
};
//______________________________________________________________________

/** Input stream which decompresses data. Analogous to Zobstream, aware of
    jigdo file formats - expects a number of DATA or BZIP parts at current
    stream position. */
class Zibstream {
public:

  /** Interface for gzip and bzip2 implementors. */
  class Impl {
  public:
    virtual ~Impl() { }

    virtual unsigned totalOut() const = 0;
    virtual unsigned totalIn() const = 0;
    virtual unsigned availOut() const = 0;
    virtual unsigned availIn() const = 0;
    virtual Ubyte* nextOut() const = 0;
    virtual Ubyte* nextIn() const = 0;
    virtual void setTotalOut(unsigned n) = 0;
    virtual void setTotalIn(unsigned n) = 0;
    virtual void setAvailIn(unsigned n) = 0;
    virtual void setNextIn(Ubyte* n) = 0;

    /** Initialize, i.e. inflateInit(). {next,avail}{in,out} must be set up
        before calling this. Does not throw, sets an internal status flag. */
    virtual void init() = 0;
    /** Finalize, i.e. inflateEnd(). Does not throw. */
    virtual void end() = 0;
    /** Re-init, i.e. inflateReset() */
    virtual void reset() = 0;

    /** Sets an internal status flag. Updates nextOut and availOut. */
    virtual void inflate(Ubyte** nextOut, unsigned* availOut) = 0;
    /** Check status flag: At stream end? */
    virtual bool streamEnd() const = 0;
    /** Check status flag: OK? */
    virtual bool ok() const = 0;

    /** Depending on internal status flag, throw appropriate Zerror. Never
        returns. */
    virtual void throwError() const = 0; /* throws Zerror */
  };
  //________________________________________

  inline explicit Zibstream(unsigned bufSz = 64*1024);
  /** Calls close(), which might throw a Zerror exception! Call
      close() before destroying the object to avoid this. */
  virtual ~Zibstream() { close(); delete buf; if (z != 0) z->end(); delete z; }
  inline Zibstream(bistream& s, unsigned bufSz = 64*1024);
  bool is_open() const { return stream != 0; }
  void close();

  /** Get reference to underlying istream */
  bistream& getStream() { return *stream; }

  /// Input 1 character
//   inline Zibstream& get(unsigned char& x);
//   inline Zibstream& get(signed char& x);
  /// Input 32 bit integer in little-endian order
  //Zibstream& get(uint32 x);
  /// Input n characters
//   inline Zibstream& read(const char* x, size_t n);
//   inline Zibstream& read(const signed char* x, size_t n);
//   Zibstream& read(const unsigned char* x, size_t n);
//   inline Zibstream& read(const void* x, size_t n);
  Zibstream& read(Ubyte* x, unsigned n);
  typedef uint64 streamsize;
  /** Number of characters read by last read() */
  inline streamsize gcount() const {
    streamsize n = gcountVal; gcountVal = 0; return n;
  }

  bool good() const { return is_open() && buf != 0; }
  bool eof() const { return dataUnc == 0; }
  bool fail() const { return !good(); }
  bool bad() const { return false; }
  operator void*() const { return fail() ? (void*)0 : (void*)(-1); }
  bool operator!() const { return fail(); }


private:
  // Throw a Zerror exception, or bad_alloc() for status==Z_MEM_ERROR
  //inline void throwZerror(int status, const char* zmsg);

  static const unsigned DATA = 0x41544144u;
  static const unsigned BZIP = 0x50495a42u;

  void open(bistream& s);

//   z_stream z;
  Impl* z;
  bistream* stream;
  mutable streamsize gcountVal;
  unsigned bufSize;
  Ubyte* buf; // Contains compressed data
  uint64 dataLen; // bytes remaining to be read from current DATA part
  uint64 dataUnc; // bytes remaining in uncompressed DATA part
  Ubyte* nextOut; // Pointer into output buffer
  unsigned availOut; // Bytes remaining in output buffer
};
//______________________________________________________________________

/* Initialising todoBufSize and todoCount in this way allows us to
   move Assert(is_open) out of the inline functions and into zip() for
   calls to put() and write() */
Zobstream::Zobstream(MD5Sum* md, SHA256Sum *sd)
    : zipBuf(0), zipBufLast(0), todoBuf(0), todoBufSize(0), todoCount(0),
      stream(0), md5sum(md), sha256sum(sd) { }
//________________________________________

void Zobstream::open(bostream& s, unsigned chunkLimit, unsigned todoBufSz) {
  Assert(!is_open());
  todoBufSize = (MIN_TODOBUF_SIZE > todoBufSz ? MIN_TODOBUF_SIZE :todoBufSz);
  chunkLimVal = chunkLimit;

  todoCount = 0;
  todoBuf = new Ubyte[todoBufSize];

  stream = &s; // Declare as open
  Paranoid(stream != 0);
}

void Zobstream::zip(Ubyte* start, unsigned len, bool finish) {
  if (len != 0 || finish)
    zip2(start, len, finish);
  todoCount = 0;
}
//________________________________________

Zobstream& Zobstream::put(unsigned char x) {
  if (todoCount >= todoBufSize) zip(todoBuf, todoCount);
  todoBuf[todoCount] = static_cast<Ubyte>(x);
  ++todoCount;
  return *this;
}

Zobstream& Zobstream::put(signed char x) {
  if (todoCount >= todoBufSize) zip(todoBuf, todoCount);
  todoBuf[todoCount] = static_cast<Ubyte>(x);
  ++todoCount;
  return *this;
}

Zobstream& Zobstream::put(char x) {
  if (todoCount >= todoBufSize) zip(todoBuf, todoCount);
  todoBuf[todoCount] = static_cast<Ubyte>(x);
  ++todoCount;
  return *this;
}

Zobstream& Zobstream::put(int x) {
  if (todoCount >= todoBufSize) zip(todoBuf, todoCount);
  todoBuf[todoCount] = static_cast<Ubyte>(x);
  ++todoCount;
  return *this;
}

Zobstream& Zobstream::write(const Ubyte* x, unsigned n) {
  Assert(is_open());
  if (n > 0) {
    zip(todoBuf, todoCount); // Zip remaining data in todoBuf
    zip(const_cast<Ubyte*>(x), n); // Zip byte array
  }
  return *this;
}
//________________________________________

Zibstream::Zibstream(unsigned bufSz)
    : z(0), stream(0), bufSize(bufSz), buf(0) {
}

Zibstream::Zibstream(bistream& s, unsigned bufSz)
    : z(0), stream(0), bufSize(bufSz), buf(0) {
  // data* will be init'ed by open()
  open(s);
}

#endif
