/* $Id: md5sum-test.cc,v 1.2 2003/09/27 21:31:04 atterer Exp $ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2000-2002  |  richard@
  | \/�|  Richard Atterer          |  atterer.org
  � '` �

  Copyright (C) 2016-2021 Steve McIntyre <steve@einval.com>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Quite secure 128-bit checksum

  #test-deps util/glibc-md5.o util/md5sum.o

*/

#include <config.h>

#include <iomanip>
#include <iostream>
#include <fstream>

#include <bstream.hh>
#include <log.hh>
#include <md5sum.hh>
#include <mimestream.hh>
//______________________________________________________________________

namespace {

int returnCode = 0;

// MD5 test suite (see end of RFC1321)

const Ubyte t1[] = "";
const Ubyte s1[] =
"\xd4\x1d\x8c\xd9\x8f\x00\xb2\x04\xe9\x80\x09\x98\xec\xf8\x42\x7e";
const Ubyte t2[] = "a";
const Ubyte s2[] =
"\x0c\xc1\x75\xb9\xc0\xf1\xb6\xa8\x31\xc3\x99\xe2\x69\x77\x26\x61";
const Ubyte t3[] = "abc";
const Ubyte s3[] =
"\x90\x01\x50\x98\x3c\xd2\x4f\xb0\xd6\x96\x3f\x7d\x28\xe1\x7f\x72";
const Ubyte t4[] = "message digest";
const Ubyte s4[] =
"\xf9\x6b\x69\x7d\x7c\xb7\x93\x8d\x52\x5a\x2f\x31\xaa\xf1\x61\xd0";
const Ubyte t5[] = "abcdefghijklmnopqrstuvwxyz";
const Ubyte s5[] =
"\xc3\xfc\xd3\xd7\x61\x92\xe4\x00\x7d\xfb\x49\x6c\xca\x67\xe1\x3b";
const Ubyte t6[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
const Ubyte s6[] =
"\xd1\x74\xab\x98\xd2\x77\xd9\xf5\xa5\x61\x1c\x2c\x9f\x41\x9d\x9f";
const Ubyte t7[] = "12345678901234567890123456789012345678901234567890123456789012345678901234567890";
const Ubyte s7[] =
"\x57\xed\xf4\xa2\x2b\xe3\xc9\x55\xac\x49\xda\x2e\x21\x07\xb6\x7a";
const Ubyte sAll[] =
"\x6f\xec\x75\xd4\xe7\xfc\xd7\xe9\x66\x46\xb4\xc7\xaf\x96\xbc\xe2";

const char* const hexDigits = "0123456789abcdef";

}
//______________________________________________________________________

string toHex(const Ubyte* sum) {
  string result;
  for (int i = 0; i < 16; ++i) {
    result += hexDigits[sum[i] >> 4];
    result += hexDigits[sum[i] & 0xfU];
  }
  return result;
}

void compare(const Ubyte* suite, const Ubyte* mine) {
  for (int i = 0; i < 16; ++i) {
    if (suite[i] != mine[i]) {
      msg("ERROR: Expected %1, but got %2", toHex(suite), toHex(mine));
      returnCode = 1;
      return;
    }
  }
  msg("OK: %1", toHex(suite));
}

void printBlockSums(size_t blockSize, const char* fileName) {
  bifstream file(fileName, ios::binary);
  Ubyte buf[blockSize];
  Ubyte* bufEnd = buf + blockSize;

  while (file) {
    // read another block
    Ubyte* cur = buf;
    while (cur < bufEnd && file) {
      readBytes(file, cur, bufEnd - cur); // Fill buffer
      cur += file.gcount();
    }
    MD5Sum sum;
    sum.update(buf, cur - buf).finishForReuse();
    cout << ' ' << sum << endl;
  }
  exit(0);
}

int main(int argc, char* argv[]) {
  if (argc == 2) Logger::scanOptions(argv[1], argv[0]);
  if (argc == 3) {
    // 2 cmdline args, blocksize and filename. Print RsyncSums of all blocks
    printBlockSums(atoi(argv[1]), argv[2]);
    exit(0);
  }
  const Ubyte* sum;
  MD5Sum x;
  MD5Sum all;

  sum = x.reset().update(t1, sizeof(t1) - 1).finish().digest();
  compare(s1, sum);
  all.update(t1, sizeof(t1) - 1);

  sum = x.reset().update(t2, sizeof(t2) - 1).finish().digest();
  compare(s2, sum);
  all.update(t2, sizeof(t2) - 1);

  sum = x.reset().update(t3, sizeof(t3) - 1).finish().digest();
  compare(s3, sum);
  all.update(t3, sizeof(t3) - 1);

  sum = x.reset().update(t4, sizeof(t4) - 1).finish().digest();
  compare(s4, sum);
  all.update(t4, sizeof(t4) - 1);

  sum = x.reset().update(t5, sizeof(t5) - 1).finish().digest();
  compare(s5, sum);
  all.update(t5, sizeof(t5) - 1);

  sum = x.reset().update(t6, sizeof(t6) - 1).finish().digest();
  compare(s6, sum);
  all.update(t6, sizeof(t6) - 1);

  sum = x.reset().update(t7, sizeof(t7) - 1).finish().digest();
  compare(s7, sum);
  all.update(t7, sizeof(t7) - 1);

  sum = all.finish().digest();
  compare(sAll, sum);

  return returnCode;
}
