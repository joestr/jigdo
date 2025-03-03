/* $Id: rsyncsum.cc,v 1.2 2003/09/27 21:31:04 atterer Exp $ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2000-2002  |  richard@
  | \/�|  Richard Atterer          |  atterer.org
  � '` �

  Copyright (C) 2016-2021 Steve McIntyre <steve@einval.com>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  A 32 or 64 bit rolling checksum

*/

#include <config.h>
#include <rsyncsum.hh>
#include <rsyncsum.ih>
//______________________________________________________________________

// NB: The following assumes that uint32 is at least 32 bits, but may be 64

/* Note: This will not yield the same results as the original rsync
   algorithm because 1) it uses a non-zero CHAR_OFFSET, and 2) it
   treats single bytes as unsigned, not signed. */
RsyncSum& RsyncSum::addBack(const Ubyte* mem, size_t len) {
  uint32 a = sum;
  uint32 b = sum >> 16;
  const Ubyte* blockLimit = mem + (len / 16) * 16;
  const Ubyte* limit = mem + len; // 1st byte not to process

  while (mem < blockLimit) {
    a += *mem++ + CHAR_OFFSET; b += a;
    a += *mem++ + CHAR_OFFSET; b += a;
    a += *mem++ + CHAR_OFFSET; b += a;
    a += *mem++ + CHAR_OFFSET; b += a;
    a += *mem++ + CHAR_OFFSET; b += a;
    a += *mem++ + CHAR_OFFSET; b += a;
    a += *mem++ + CHAR_OFFSET; b += a;
    a += *mem++ + CHAR_OFFSET; b += a;
    a += *mem++ + CHAR_OFFSET; b += a;
    a += *mem++ + CHAR_OFFSET; b += a;
    a += *mem++ + CHAR_OFFSET; b += a;
    a += *mem++ + CHAR_OFFSET; b += a;
    a += *mem++ + CHAR_OFFSET; b += a;
    a += *mem++ + CHAR_OFFSET; b += a;
    a += *mem++ + CHAR_OFFSET; b += a;
    a += *mem++ + CHAR_OFFSET; b += a;
  }

  while (mem < limit) {
    a += *mem++ + CHAR_OFFSET; b += a;
  }

  sum = ((a & 0xffff) + (b << 16)) & 0xffffffff;
  return *this;
}
//________________________________________

RsyncSum& RsyncSum::removeFront(const Ubyte* mem, size_t len,
                                size_t areaSize) {
  RsyncSum front(mem, len);
  unsigned long a = sum;
  unsigned long b = sum >> 16;
  /* The following is much cheaper than subtracting
     (areaSize-i)*(mem[i]+CHAR_OFFSET) from b for each i, 0<=i<len,
     because only one multiplication is needed, not len. */
  a -= front.get();
  b -= (front.get() >> 16) + (areaSize - len) * front.get();
  sum = ((a & 0xffff) + (b << 16)) & 0xffffffff;
  return *this;
}
//______________________________________________________________________

RsyncSum64& RsyncSum64::addBack2(const Ubyte* mem, size_t len) {
  uint32 a = sumLo;
  uint32 b = sumHi;
  const Ubyte* blockLimit = mem + (len / 16) * 16;
  const Ubyte* limit = mem + len; // 1st byte not to process

  while (mem < blockLimit) {
    a += charTable[*mem++]; b += a;
    a += charTable[*mem++]; b += a;
    a += charTable[*mem++]; b += a;
    a += charTable[*mem++]; b += a;
    a += charTable[*mem++]; b += a;
    a += charTable[*mem++]; b += a;
    a += charTable[*mem++]; b += a;
    a += charTable[*mem++]; b += a;
    a += charTable[*mem++]; b += a;
    a += charTable[*mem++]; b += a;
    a += charTable[*mem++]; b += a;
    a += charTable[*mem++]; b += a;
    a += charTable[*mem++]; b += a;
    a += charTable[*mem++]; b += a;
    a += charTable[*mem++]; b += a;
    a += charTable[*mem++]; b += a;
  }

  while (mem < limit) {
    a += charTable[*mem++]; b += a;
  }

  sumLo = a & 0xffffffff;
  sumHi = b & 0xffffffff;
  return *this;
}
//________________________________________

RsyncSum64& RsyncSum64::removeFront(const Ubyte* mem, size_t len,
                                    size_t areaSize) {
  RsyncSum64 front(mem, len);
  sumLo = (sumLo - front.getLo()) & 0xffffffff;
  sumHi = (sumHi - front.getHi() - (areaSize - len) * front.getLo())
    &0xffffffff;
  return *this;
}
//________________________________________

/* These are purely random, no patterns or anything... (I hope)

   I do not claim copyright for the actual numbers below, you may use them
   for a re-implementation of the algorithm under a license of your choice.
   -- Richard Atterer. */
const uint32 RsyncSum64::charTable[256] = {
  0x51d65c0f, 0x083cd94b, 0x77f73dd8, 0xa0187d36,
  0x29803d07, 0x7ea8ac0e, 0xea4c16c9, 0xfc576443,
  0x6213df29, 0x1c012392, 0xb38946ae, 0x2e20ca31,
  0xe4dc532f, 0xcb281c47, 0x8508b6a5, 0xb93c210d,
  0xef02b5f3, 0x66548c74, 0x9ae2deab, 0x3b59f472,
  0x4e546447, 0x45232d1f, 0x0ac0a4b1, 0x6c4c264b,
  0x5d24ce84, 0x0f2752cc, 0xa35c7ac7, 0x3e31af51,
  0x79675a59, 0x581f0e81, 0x49053122, 0x7339c9d8,
  0xf9833565, 0xa3dbe5b3, 0xcc06eeb9, 0x92d0671c,
  0x3eb220a7, 0x64864eae, 0xca100872, 0xc50977a1,
  0xd90378e1, 0x7a36cab9, 0x15c15f4b, 0x8b9ef749,
  0xcc1432dc, 0x1ec578ed, 0x27e6e092, 0xbb06db8f,
  0x67f661ac, 0x8dd1a3db, 0x2a0ca16b, 0xb229ab84,
  0x127a3337, 0x347d846f, 0xe1ea4b50, 0x008dbb91,
  0x414c1426, 0xd2be76f0, 0x08789a39, 0xb4d93e30,
  0x61667760, 0x8871bee9, 0xab7da12d, 0xe3c58620,
  0xe9fdfbbe, 0x64fb04f7, 0x8cc5bbf0, 0xf5272d30,
  0x8f161b50, 0x11122b05, 0x7695e72e, 0xa1c5d169,
  0x1bfd0e20, 0xef7e6169, 0xf652d08e, 0xa9d0f139,
  0x2f70aa04, 0xae2c7d6d, 0xa3cb9241, 0x3ae7d364,
  0x348788f8, 0xf483b8f1, 0x55a011da, 0x189719dc,
  0xb0c5d723, 0x8b344e33, 0x300d46eb, 0xd44fe34f,
  0x1a2016c1, 0x66ce4cd7, 0xa45ea5e3, 0x55cb708a,
  0xbce430df, 0xb01ae6e0, 0x3551163b, 0x2c5b157a,
  0x574c4209, 0x430fd0e4, 0x3387e4a5, 0xee1d7451,
  0xa9635623, 0x873ab89b, 0xb96bc6aa, 0x59898937,
  0xe646c6e7, 0xb79f8792, 0x3f3235d8, 0xef1b5acf,
  0xd975b22b, 0x427acce6, 0xe47a2411, 0x75f8c1e8,
  0xa63f799d, 0x53886ad8, 0x9b2d6d32, 0xea822016,
  0xcdee2254, 0xd98bcd98, 0x2933a544, 0x961f379f,
  0x49219792, 0xc61c360f, 0x77cc0c64, 0x7b872046,
  0xb91c7c12, 0x7577154b, 0x196573be, 0xf788813f,
  0x41e2e56a, 0xec3cd244, 0x8c7401f1, 0xc2e805fe,
  0xe8872fbe, 0x9e2faf7d, 0x6766456b, 0x888e2197,
  0x28535c6d, 0x2ce45f3f, 0x24261d2a, 0xd6faab8b,
  0x7a7b42b8, 0x15f0f6fa, 0xfe1711df, 0x7e5685a6,
  0x00930268, 0x74755331, 0x1998912c, 0x7b60498b,
  0x501a5786, 0x92ace0f6, 0x1d9752fe, 0x5a731add,
  0x5b3b44fc, 0x473673f9, 0xa42c0321, 0xd82f9f18,
  0xb4b225da, 0xfc89ece2, 0x072e1130, 0x5772aae3,
  0x29010857, 0x542c970c, 0x94f67fe5, 0x71209e9b,
  0xdb97ea39, 0x2689b41b, 0xae815804, 0xfc5e2651,
  0xd4521674, 0x48ed979a, 0x2f617da3, 0xc350353d,
  0xc3accd94, 0xbd8d313a, 0xc61a8e77, 0xf34940a4,
  0x8d2c6b0f, 0x0f0e7225, 0x39e183db, 0xd19ebba9,
  0x6a0f37b9, 0xd18922f3, 0x106420c5, 0xaa5a640b,
  0x7cf0d273, 0xcf3238a7, 0x3b33204f, 0x476be7bb,
  0x09d23bca, 0xbe84b2f7, 0xb7a3bace, 0x2528cee1,
  0x3dcaa1dd, 0x900ad31a, 0xf21dea6d, 0x9ce51463,
  0xf1540bba, 0x0fab1bdd, 0x89cfb79a, 0x01a2a6e6,
  0x6f85d67c, 0xd1669ec4, 0x355db722, 0x00ebd5c4,
  0x926eb385, 0x69ead869, 0x0da2b122, 0x402779fe,
  0xdaed92d0, 0x57e9aabb, 0x3df64854, 0xfcc774b5,
  0x2e1740ed, 0xa615e024, 0xf7bac938, 0x377dfd1a,
  0xd0559d66, 0x25499be8, 0x2d8f2006, 0xfaa9e486,
  0x95e980e7, 0x82aeba67, 0x5a7f2561, 0xbc60dff6,
  0x6c8739a2, 0x7ec59a8b, 0x9998f265, 0xdfe37e5e,
  0xb47cee1e, 0x4dd8bc9e, 0x35c57e09, 0x07850b63,
  0x06eadbcb, 0x6c1f2956, 0x01685c2c, 0xf5725eef,
  0xf13b98b5, 0xaab739c2, 0x200b1da2, 0xa716b98b,
  0xd9ee3058, 0x76acf20b, 0x2f259e04, 0xed11658b,
  0x1532b331, 0x0ab43204, 0xf0beb023, 0xb1685483,
  0x58cbdc4f, 0x079384d3, 0x049b141c, 0xc38184b9,
  0xaf551d9a, 0x66222560, 0x059deeca, 0x535f99e2
};
