#! /usr/bin/env awk -f
#  __   _
#  |_) /|  Copyright (C) 2001 Richard Atterer
#  | \/�|  <richard@atterer.net>
#  � '` �
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License, version 2. See
#  the file COPYING for details.

# Syntax: glade-filter <version-number> interface.cc.tmp

# where interface.cc.tmp was generated by glade. Will post-process
# that file to generate a simple class that contains pointers to all the
# widgets, and the create_ functions, so you don't need to look up
# widgets by name.

# @VERSION@ anywhere in the glade file is replaced with <version-number>

# Overwrites interface.cc and interface.hh
#______________________________________________________________________

# Add code line to hh, with right indentation
function addhh(string) {
  sub(/^ +/, "", string);
  if (substr(string, 1, 1) == "}") hhindent = substr(hhindent, 3);
  string = hhindent string;
  sub(/ +$/, "", string);
  hh = hh string "\n";
  if (substr(string, length(string)) == "{") hhindent = hhindent "  ";
}

function addcc(string) {
  cc = cc string "\n";
}
#______________________________________________________________________

BEGIN {
  version = ARGV[1];
  delete ARGV[1];

  stem = ARGV[2];
  leaf = stem;
  gsub(/\.cc?\.tmp$/, "", stem);
  gsub(/.*\//, "", leaf);
  if (stem == ARGV[1]) {
    print "Syntax: glade-filter.awk interface.cc.tmp";
    exit(1);
  }
  outputcc = stem".cc";
  outputhh = stem".hh";
  hhGuard = toupper(outputhh);
  gsub(/.*\//, "", hhGuard);
  gsub(/[^A-Z0-9]/, "_", hhGuard);
  namespace = "GUI";

  cc = "// Automatically created from `"leaf"' by glade-filter.awk\n\n";
  hh = cc;
  #addcc("#include <"outputhh">");
  addhh("#ifndef "hhGuard);
  addhh("#define "hhGuard);
  addhh("");
  addhh("#include <config.h>");
  addhh("#include <gtk/gtk.h>");
  addhh("");
  addhh("namespace "namespace" {");
  collectDecl = 0;
}
#______________________________________________________________________

/^#include / {
  sub(/ "/, " <");
  sub(/"$/, ">");
  sub(/\.cc?\.tmp/, ".cc");
  sub(/\.hh?\.tmp/, ".hh");
}

/@VERSION@/ {
  gsub(/@VERSION@/, version);
}

/^create_[a-zA-Z]+ *\( *(void *)?\) *$/ {
  # New function definition => new GUI class
  guiElem = toupper(substr($0, 8, 1)) substr($0, 9, index($0, " ") - 9);
  addcc(namespace"::"guiElem"::create()");
  addhh("");
  addhh("struct "guiElem" {");
  addhh(prevLine" create();");
  collectDecl = 1;
  next;
}

/^ *} *$/ {
  if (length(guiElem) != 0) {
    # End of function definition
    addhh("};");
    guiElem = "";
  }
}

/^ *Gtk[a-zA-Z0-9_]+ +\*? *[a-zA-Z0-9_]+; *$/ {
  if (collectDecl) {
    # GtkSomething declaration; - remove from cc, add to hh inside class
    addhh($0);
    next;
  }
}

/(^ *$)|=/ { collectDecl = 0; }

{
  cc = cc $0 "\n"; # By default, copy over code into output file
  prevLine = $0;
}
#______________________________________________________________________

END {
  addhh("");
  addhh("} // namespace "namespace);
  addhh("");
  addhh("#endif /* "hhGuard" */");

  # To avoid recompilation, only update .cc if contents changed
  old = "";
  while ((getline line < outputcc) == 1) old = old line "\n";
  if (old == cc) print "`"outputcc"' is unchanged";
  else printf("%s", cc) > outputcc;

  # To avoid recompilation, only update .hh if contents changed
  old = "";
  while ((getline line < outputhh) == 1) old = old line "\n";
  if (old == hh) print "`"outputhh"' is unchanged";
  else printf("%s", hh) > outputhh;
}
