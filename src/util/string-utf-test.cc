/* $Id: string-utf-test.cc,v 1.1 2003/09/16 23:32:10 atterer Exp $ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/�|  Richard Atterer     |  atterer.org
  � '` �
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Helper functions for dealing with UTF-8 strings

  #test-deps

*/

#include <config.h>

#include <glib.h>
#include <stdlib.h>
#include <iostream>
#include <string>

#include <log.hh>
#include <string-utf.hh>
//______________________________________________________________________

namespace {

int returnCode = 0;

void test(const char* correct, const string& generated) {
  if (generated == correct) {
    msg("OK: \"%1\"", generated);
    return;
  }
  msg("FAILED:");
  msg("  expected \"%1\"", correct);
  msg("  but got  \"%1\"", generated);
  returnCode = 1;
}

}
//____________________

int main(int argc, char* argv[]) {
  if (argc == 2) Logger::scanOptions(argv[1], argv[0]);
  /*
  const char* charset;
  g_get_charset(&charset);
  cout << "glib thinks that your locale uses the following character "
    "encoding: " << charset << "\nI'm setting CHARSET=ISO-8859-15 just for "
    "this test\n";
  */

  // For this test, make glib assume 8-bit locale
  //putenv("CHARSET=ISO-8859-15");

  string s("string");
  test("1 >42 foo 43% 666 1024 X © string string ",
       subst("1 >%1 foo %2%% %3 %4 %5 %6 %7 %8 %9",
             42, 43U, 666L, 1024UL, 'X', "©", s, &s));

  // Arg not valid UTF-8, so it is cut off at first invalid char
  test("2 &Wah <>&W Woo!",
       subst("2 &Wah %1 Woo!", "<>&W���h"));

  // As above, but escape <>&
  test("3 &Wah &lt;&gt;&amp;W Woo!",
       subst("3 &Wah %E1 Woo!", "<>&W���h"));
  // Let's try this again with a valid UTF-8 string
  test("4 &Wah Wä&lt;©&gt;&amp;h Woo!",
       subst("4 &Wah %E1 Woo!", "Wä<©>&h"));

  // When using F, the thing is not checked, producing invalid UTF-8
  test("5 Wah W���h <Woo!",
       subst("5 Wah %F1 <Woo!", "W���h"));

  // But we can have subst() convert the string for us
  test("6 Wah Wäääh 0xdeadbeef 1.234500",
       subst("6 Wah %L1 %2 %3", "W���h", (void*)(0xDeadBeef), 1.2345));

  // convert from ISO-8859-1 and escape <>&
  test("7 Wah <b>Wä&lt;ä&gt;äh&amp;</b> &lt; Woo!",
       subst("7 Wah <b>%FELL1</b> &lt; Woo!", "W�<�>�h&"));

  // escape, but assume that arg is UTF-8
  test("8 Wah <b>Wo&lt;o&gt;oh&amp;</b> &lt; Woo!",
       subst("8 Wah <b>%FE1</b> &lt; Woo!", "Wo<o>oh&"));

  return returnCode;
}
