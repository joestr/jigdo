/* $Id: sha256sum.cc,v 1.4 2004/06/20 20:35:15 atterer Exp $ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2000-2004  |  richard@
  | \/¯|  Richard Atterer          |  atterer.org
  ¯ '` ¯
  "Ported" to C++ by RA. Uses glibc code for the actual algorithm.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Quite secure 128-bit checksum

*/

#include <config.h>

#include <iostream>
#include <vector>

#include <glibc-sha256.hh>
#include <sha256sum.hh>
#include <sha256sum.ih>
//______________________________________________________________________

void SHA256Sum::ProgressReporter::error(const string& message) {
  cerr << message << endl;
}
void SHA256Sum::ProgressReporter::info(const string& message) {
  cerr << message << endl;
}
void SHA256Sum::ProgressReporter::readingSHA256(uint64, uint64) { }

SHA256Sum::ProgressReporter SHA256Sum::noReport;
//______________________________________________________________________

SHA256Sum::SHA256Sum(const SHA256Sum& md) {
  if (md.p == 0) {
    p = 0;
    for (int i = 0; i < 32; ++i) sum[i] = md.sum[i];
  } else {
    p = new sha256_ctx();
    *p = *md.p;
  }
}
//________________________________________

// NB must work with self-assign
SHA256Sum& SHA256Sum::operator=(const SHA256Sum& md) {
# if DEBUG
  finished = md.finished;
# endif
  if (md.p == 0) {
    delete p;
    p = 0;
    for (int i = 0; i < 32; ++i) sum[i] = md.sum[i];
  } else {
    if (p == 0) p = new sha256_ctx();
    *p = *md.p;
  }
  return *this;
}
//______________________________________________________________________

string SHA256::toString() const {
  Base64String m;
  m.write(sum, 32).flush();
  return m.result();
}
//______________________________________________________________________

bool SHA256::operator_less2(const SHA256& x) const {
  if (sum[1] < x.sum[1]) return true;
  if (sum[1] > x.sum[1]) return false;
  if (sum[2] < x.sum[2]) return true;
  if (sum[2] > x.sum[2]) return false;
  if (sum[3] < x.sum[3]) return true;
  if (sum[3] > x.sum[3]) return false;
  if (sum[4] < x.sum[4]) return true;
  if (sum[4] > x.sum[4]) return false;
  if (sum[5] < x.sum[5]) return true;
  if (sum[5] > x.sum[5]) return false;
  if (sum[6] < x.sum[6]) return true;
  if (sum[6] > x.sum[6]) return false;
  if (sum[7] < x.sum[7]) return true;
  if (sum[7] > x.sum[7]) return false;
  if (sum[8] < x.sum[8]) return true;
  if (sum[8] > x.sum[8]) return false;
  if (sum[9] < x.sum[9]) return true;
  if (sum[9] > x.sum[9]) return false;
  if (sum[10] < x.sum[10]) return true;
  if (sum[10] > x.sum[10]) return false;
  if (sum[11] < x.sum[11]) return true;
  if (sum[11] > x.sum[11]) return false;
  if (sum[12] < x.sum[12]) return true;
  if (sum[12] > x.sum[12]) return false;
  if (sum[13] < x.sum[13]) return true;
  if (sum[13] > x.sum[13]) return false;
  if (sum[14] < x.sum[14]) return true;
  if (sum[14] > x.sum[14]) return false;
  if (sum[15] < x.sum[15]) return true;
  if (sum[15] > x.sum[15]) return false;
  if (sum[16] < x.sum[16]) return true;
  if (sum[16] > x.sum[16]) return false;
  if (sum[17] < x.sum[17]) return true;
  if (sum[17] > x.sum[17]) return false;
  if (sum[18] < x.sum[18]) return true;
  if (sum[18] > x.sum[18]) return false;
  if (sum[19] < x.sum[19]) return true;
  if (sum[19] > x.sum[19]) return false;
  if (sum[20] < x.sum[20]) return true;
  if (sum[20] > x.sum[20]) return false;
  if (sum[21] < x.sum[21]) return true;
  if (sum[21] > x.sum[21]) return false;
  if (sum[22] < x.sum[22]) return true;
  if (sum[22] > x.sum[22]) return false;
  if (sum[23] < x.sum[23]) return true;
  if (sum[23] > x.sum[23]) return false;
  if (sum[24] < x.sum[24]) return true;
  if (sum[24] > x.sum[24]) return false;
  if (sum[25] < x.sum[25]) return true;
  if (sum[25] > x.sum[25]) return false;
  if (sum[26] < x.sum[26]) return true;
  if (sum[26] > x.sum[26]) return false;
  if (sum[27] < x.sum[27]) return true;
  if (sum[27] > x.sum[27]) return false;
  if (sum[28] < x.sum[28]) return true;
  if (sum[28] > x.sum[28]) return false;
  if (sum[29] < x.sum[29]) return true;
  if (sum[29] > x.sum[29]) return false;
  if (sum[30] < x.sum[30]) return true;
  if (sum[30] > x.sum[30]) return false;
  if (sum[31] < x.sum[31]) return true;
  return false;
}
//______________________________________________________________________

uint64 SHA256Sum::updateFromStream(bistream& s, uint64 size, size_t bufSize,
                                ProgressReporter& pr) {
  uint64 nextReport = REPORT_INTERVAL; // When next to call reporter
  uint64 toRead = size;
  uint64 bytesRead = 0;
  vector<byte> buffer;
  buffer.resize(bufSize);
  byte* buf = &buffer[0];
  // Read from stream and update *this
  while (s && !s.eof() && toRead > 0) {
    size_t n = (size_t)(toRead < bufSize ? toRead : bufSize);
    readBytes(s, buf, n);
    n = s.gcount();
    update(buf, n);
    bytesRead += n;
    toRead -= n;
    if (bytesRead >= nextReport) {
      pr.readingSHA256(bytesRead, size);
      nextReport += REPORT_INTERVAL;
    }
  }
  return bytesRead;
}
