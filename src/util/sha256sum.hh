/*

  "Ported" to C++ by steve. Uses glibc code for the actual algorithm.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Quite secure 256-bit checksum

*/

#ifndef SHA256SUM_HH
#define SHA256SUM_HH

#ifndef INLINE
#  ifdef NOINLINE
#    define INLINE
#    else
#    define INLINE inline
#  endif
#endif

#include <config.h>

#include <cstdlib>
#include <cstring>
#include <iosfwd>
#include <string>

#include <bstream.hh>
#include <debug.hh>
#include <sha256sum.fh>
//______________________________________________________________________

/** Container for an already computed SHA256Sum.

    Objects of this class are smaller than SHA256Sum objects by one
    pointer. As soon as the checksum calculation of an SHA256Sum object
    has finish()ed, the pointer is no longer needed. If you need to
    store a large number of calculated SHA256Sums, it may be beneficial
    to assign the SHA256Sum to an SHA256 to save space. */
class SHA256 {
public:
  SHA256() { }
  inline SHA256(const SHA256Sum& md);
  /** 32 bytes of SHA256 checksum */
  byte sum[32];
  /** Allows you to treat the object exactly like a pointer to a byte
      array */
  operator byte*() { return sum; }
  operator const byte*() const { return sum; }
  /** Assign an SHA256Sum */
  inline SHA256& operator=(const SHA256Sum& md);
  inline bool operator<(const SHA256& x) const;
  /** Clear contents to zero */
  inline SHA256& clear();
  /** Convert to string */
  string toString() const;

  template<class Iterator>
  inline Iterator serialize(Iterator i) const;
  template<class ConstIterator>
  inline ConstIterator unserialize(ConstIterator i);
  inline size_t serialSizeOf() const { return 32; }

  // Default copy ctor
private:
  bool operator_less2(const SHA256& x) const;
  static const byte zero[32];
};

inline bool operator==(const SHA256& a, const SHA256& b);
inline bool operator!=(const SHA256& a, const SHA256& b) { return !(a == b); }

/// Output SHA256 as Base64 digest
INLINE ostream& operator<<(ostream& s, const SHA256& r);
//______________________________________________________________________

/** A 256-bit, cryptographically strong message digest algorithm.

    Unless described otherwise, if a method returns an SHA256Sum&, then
    this is a reference to the object itself, to allow chaining of
    calls. */
class SHA256Sum {
  friend class SHA256;
public:
  class ProgressReporter;

  /** Initialise the checksum */
  inline SHA256Sum();
  /** Initialise with another checksum instance */
  SHA256Sum(const SHA256Sum& md);
  ~SHA256Sum() { delete p; }
  /** Assign another checksum instance */
  SHA256Sum& operator=(const SHA256Sum& md);
  /** Tests for equality. Note: Will only return true if both message
      digest operations have been finished and their SHA256 sums are the
      same. */
  inline bool operator==(const SHA256Sum& md) const;
  inline bool operator!=(const SHA256Sum& md) const;
  inline bool operator==(const SHA256& md) const { return sum == md; }
  inline bool operator!=(const SHA256& md) const { return sum != md; }
  /** Reset checksum object to the same state as immediately after its
      creation. You must call when reusing an SHA256Sum object - call it
      just before the first update() for the new checksum. */
  inline SHA256Sum& reset();
  /** Process bytes with the checksum algorithm. May lead to some
      bytes being temporarily buffered internally. */
  inline SHA256Sum& update(const byte* mem, size_t len);
  /// Add a single byte. NB, not implemented efficiently ATM
  inline SHA256Sum& update(byte x) { update(&x, 1); return *this; }
  /** Process remaining bytes in internal buffer and create the final
      checksum.
      @return Pointer to the 32-byte checksum. */
  inline SHA256Sum& finish();
  /** Exactly the same behaviour as finish(), but is more efficient if
      you are going to call reset() again in the near future to re-use
      the SHA256Sum object.
      @return Pointer to the 32-byte checksum. */
  inline SHA256Sum& finishForReuse();
  /** Deallocate buffers like finish(), but don't generate the final
      sum */
  inline SHA256Sum& abort();
  /** Return 32 byte buffer with checksum. Warning: Returns junk if
      checksum not yet finish()ed or flush()ed. */
  inline const byte* digest() const;

  /** Convert to string */
  INLINE string toString() const;

  /** Read data from file and update() this checksum with it.
      @param s The stream to read from
      @param size Total number of bytes to read
      @param bufSize Size of temporary read buffer
      @param pr Reporter object
      @return Number of bytes read (==size if no error) */
  uint64 updateFromStream(bistream& s, uint64 size,
      size_t bufSize = 128*1024, ProgressReporter& pr = noReport);

  /* Serializing an SHA256Sum is only allowed after finish(). The
     serialization is compatible with that of SHA256. */
  template<class Iterator>
  inline Iterator serialize(Iterator i) const;
  template<class ConstIterator>
  inline ConstIterator unserialize(ConstIterator i);
  inline size_t serialSizeOf() const { return sum.serialSizeOf(); }

private:
/* Structure to save state of computation between the single steps.  */
  struct sha256_ctx
  {
    uint32_t H[8];
    union
    {
      uint64_t total64;
#define TOTAL64_low (1 - (BYTE_ORDER == LITTLE_ENDIAN))
#define TOTAL64_high (BYTE_ORDER == LITTLE_ENDIAN)
      uint32_t total[2];
    };
    uint32_t buflen;
    union
    {
      char buffer[128];
      uint32_t buffer32[32];
      uint64_t buffer64[16];
    };
  };

  /// Default reporter: Only prints error messages to stderr
  static ProgressReporter noReport;

  // These functions are the original glibc API
  static void sha256_init_ctx(sha256_ctx* ctx);
  static void sha256_process_bytes(const void* buffer, size_t len,
                                struct sha256_ctx* ctx);
  static byte* sha256_finish_ctx(struct sha256_ctx* ctx, byte* resbuf);
  static byte* sha256_read_ctx(const sha256_ctx *ctx, byte* resbuf);
  static void sha256_process_block(const void* buffer, size_t len,
                                sha256_ctx* ctx);
  SHA256 sum;
  struct sha256_ctx* p; // null once MD creation is finished

# if DEBUG
  /* After finish(ForReuse)(), must call reset() before the next
     update(). OTOH, must only call digest() after finish(). Enforce
     this rule by keeping track of the state. */
  bool finished;
# endif
};

inline bool operator==(const SHA256& a, const SHA256Sum& b) { return b == a; }
inline bool operator!=(const SHA256& a, const SHA256Sum& b) { return b != a; }

/// Output SHA256Sum as Base64 digest
INLINE ostream& operator<<(ostream& s, const SHA256Sum& r);
//______________________________________________________________________

/** Class allowing JigdoCache to convey information back to the
    creator of a JigdoCache object. */
class SHA256Sum::ProgressReporter {
public:
  virtual ~ProgressReporter() { }
  /// General-purpose error reporting.
  virtual void error(const string& message);
  /// Like error(), but for purely informational messages.
  virtual void info(const string& message);
  /// Called when data is read during updateFromStream()
  virtual void readingSHA256(uint64 offInStream, uint64 size);
};
//______________________________________________________________________

bool SHA256Sum::operator==(const SHA256Sum& md) const {
# if DEBUG
  Paranoid(this->finished && md.finished);
# endif
  return sum == md.sum;
}
bool SHA256Sum::operator!=(const SHA256Sum& md) const {
# if DEBUG
  Paranoid(this->finished && md.finished);
# endif
  return sum != md.sum;
}

SHA256Sum::SHA256Sum() {
  p = new sha256_ctx();
  sha256_init_ctx(p);
# if DEBUG
  finished = false;
# endif
}

SHA256Sum& SHA256Sum::reset() {
  if (p == 0) p = new sha256_ctx();
  sha256_init_ctx(p);
# if DEBUG
  finished = false;
# endif
  return *this;
}

SHA256Sum& SHA256Sum::update(const byte* mem, size_t len) {
  Paranoid(p != 0);
# if DEBUG
  Paranoid(!finished); // Don't forget to call reset() before update()
# endif
  sha256_process_bytes(mem, len, p);
  return *this;
}

SHA256Sum& SHA256Sum::finish() {
  Paranoid(p != 0 );
  sha256_finish_ctx(p, sum);
  delete p;
  p = 0;
# if DEBUG
  finished = true;
# endif
  return *this;
}

SHA256Sum& SHA256Sum::finishForReuse() {
  Paranoid(p != 0  );
  sha256_finish_ctx(p, sum);
# if DEBUG
  finished = true;
# endif
  return *this;
}

SHA256Sum& SHA256Sum::abort() {
  delete p;
  p = 0;
# if DEBUG
  finished = false;
# endif
  return *this;
}

const byte* SHA256Sum::digest() const {
# if DEBUG
  Paranoid(finished); // Call finish() first
# endif
  return sum.sum;
}

template<class Iterator>
inline Iterator SHA256Sum::serialize(Iterator i) const {
# if DEBUG
  Paranoid(finished ); // Call finish() first
# endif
  return sum.serialize(i);
}
template<class ConstIterator>
inline ConstIterator SHA256Sum::unserialize(ConstIterator i) {
# if DEBUG
  finished = true;
# endif
  return sum.unserialize(i);
}
//____________________

SHA256& SHA256::operator=(const SHA256Sum& md) {
# if DEBUG
  Paranoid(md.finished); // Call finish() first
# endif
  *this = md.sum;
  return *this;
}

bool SHA256::operator<(const SHA256& x) const {
  if (sum[0] < x.sum[0]) return true;
  if (sum[0] > x.sum[0]) return false;
  return operator_less2(x);
}

// inline bool operator<(const SHA256& a, const SHA256& b) {
//   return a.operator<(b);
// }

SHA256::SHA256(const SHA256Sum& md) { *this = md.sum; }

bool operator==(const SHA256& a, const SHA256& b) {
  // How portable is this?
  return memcmp(a.sum, b.sum, 32 * sizeof(byte)) == 0;
# if 0
  return a.sum[0] == b.sum[0] && a.sum[1] == b.sum[1]
    && a.sum[2] == b.sum[2] && a.sum[3] == b.sum[3]
    && a.sum[4] == b.sum[4] && a.sum[5] == b.sum[5]
    && a.sum[6] == b.sum[6] && a.sum[7] == b.sum[7]
    && a.sum[8] == b.sum[8] && a.sum[9] == b.sum[9]
    && a.sum[10] == b.sum[10] && a.sum[11] == b.sum[11]
    && a.sum[12] == b.sum[12] && a.sum[13] == b.sum[13]
    && a.sum[14] == b.sum[14] && a.sum[15] == b.sum[15];
# endif
}

SHA256& SHA256::clear() {
  byte* x = sum;
  *x++ = 0; *x++ = 0; *x++ = 0; *x++ = 0;
  *x++ = 0; *x++ = 0; *x++ = 0; *x++ = 0;
  *x++ = 0; *x++ = 0; *x++ = 0; *x++ = 0;
  *x++ = 0; *x++ = 0; *x++ = 0; *x++ = 0;
  return *this;
}

template<class Iterator>
inline Iterator SHA256::serialize(Iterator i) const {
  for (int j = 0; j < 32; ++j) { *i = sum[j]; ++i; }
  return i;
}
template<class ConstIterator>
inline ConstIterator SHA256::unserialize(ConstIterator i) {
  for (int j = 0; j < 32; ++j) { sum[j] = *i; ++i; }
  return i;
}

#ifndef NOINLINE
#  include <sha256sum.ih> /* NOINLINE */
#endif

#endif
