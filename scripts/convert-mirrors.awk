#! /usr/bin/gawk -f
#  __   _
#  |_) /|  Copyright (C) 2001 Richard Atterer
#  | \/¯|  <richard@atterer.net>
#  ¯ '` ¯
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License, version 2. See
#  the file COPYING for details.

# Convert the list of Debian mirrors as published on Debian web
# servers into entries for the [Servers] section of a .jigdo file.
# Unfortunately, these mirror lists are not intended to be
# machine-readable...

BEGIN {
  # Add guesses about directories to output list
  guess = 0;
  outFile = ARGV[2]; delete ARGV[2];
}

function jigdo(str, comment) {
  if (str in recorded) return;
  recorded[str];
  result = str;
  #result = substr(str, 7)"   "substr(str, 1, 6);
  if (comment) {
    result = result substr("                                              " \
      "          ", length(str)) " # " comment;
  }
  print result > outFile;
}

function word(str, offset) {
  result = substr($0, offset);
  nextSpace = index(result, " ");
  if (nextSpace == 0)
    return result;
  else
    return substr(result, 1, nextSpace - 1);
}

# Start of primary mirror list
/Country +Site +Debian archive +Debian non-US archive/ {
  country = index($0, "Country");
  site = index($0, "Site");
  debian = index($0, "Debian archive");
  nonus = index($0, "Debian non-US archive");
}

# An entry in the primary mirror list
(substr($0, site-1, 2)substr($0, debian-1, 2) ~ / [a-z] \//) {
  comment = substr($0, country, site-country); # comment = country name
  gsub(/ +$/, "", comment); # strip trailing space characters
  jigdo("Debian=ftp://" word($0, site) word($0, debian), comment);
  if (substr($0, nonus-1, 2) == " /")
    jigdo("Non-US=ftp://" word($0, site) word($0, nonus), comment);
}

# Start of secondary mirror list
/HOSTNAME +FTP +HTTP/ {
  site = index($0, "HOSTNAME");
  ftp = index($0, "FTP");
  http = index($0, "HTTP");
  comment = "";
}

# Gather country name while in secondary list
/^-+$/ { comment = previous; next; }
{ previous = $0; }

# An entry in the secondary mirror list
(substr($0, site, 1)substr($0, ftp-1, 2)substr($0, http-1, 2) \
 ~ /[^ ] [^ ]|[^ ]   [^ ]/) {
  thisSite = word($0, site);
  thisHttp = word($0, ftp);
  thisFtp = word($0, http);
  if (substr(thisHttp, length(thisHttp)) != "/") thisHttp = thisHttp"/";
  if (substr(thisFtp, length(thisFtp)) != "/") thisFtp = thisFtp"/";
  # The secondary mirror list doesn't have entries for non-US. Add some
  # guesses, if they're wrong, they will be weeded out by
  # check-mirrors
  # 1) It's in the same directory
  # 2) If dir ends in "/debian/", try replacing that with "/debian-non-US/"
  #    and "/debian/non-US/"
  # 3) Otherwise, try appending "debian-non-US/" and "non-US/"
  #    Also, try replacing *any* "debian" with "debian-non-US" as a
  #    last resort
  if (substr($0, ftp-1, 2) == " /") {
    url = "ftp://" thisSite thisFtp;
    jigdo("Debian="url, comment);
    if (guess) {
      jigdo("Non-US="url, comment);
      jigdo("Non-US="url"non-US/", comment);
      jigdo("Non-US="url"debian-non-US/", comment);
      if (substr(url, length(url) - 7) == "/debian/") {
        jigdo("Non-US="substr(url, 1, length(url) - 1)"-non-US/", comment);
      } else {
        sub(/debian/, "debian-non-US", url);
        jigdo("Non-US="url, comment);
      }
    }
  }
  if (substr($0, http-1, 2) == " /") {
    url = "http://" thisSite thisHttp;
    jigdo("Debian="url, comment);
    if (guess) {
      jigdo("Non-US="url, comment);
      jigdo("Non-US="url"non-US/", comment);
      jigdo("Non-US="url"debian-non-US/", comment);
      if (substr(url, length(url) - 7) == "/debian/") {
        jigdo("Non-US="substr(url, 1, length(url) - 1)"-non-US/", comment);
      } else {
        sub(/debian/, "debian-non-US", url);
        jigdo("Non-US="url, comment);
      }
    }
  }
}

# There actually *does* exist a list of non-US mirrors, in
# README.non-US. Oh well...
/Debian GNU\/Linux non-US packages/ { inNonUS = 1; }
/^Last modified:/ { inNonUS = 0; }
/:$/ { nonUsCountry = substr($0, 1, length($0) - 1); }

/^ *(ftp|http):\/\/[a-zA-Z0-9.\/_+,:@-]+ *$/ {
  if (inNonUS) {
    if (substr($1, length($1)) != "/") $1 = $1"/";
    jigdo("Non-US="$1, nonUsCountry);
    if (guess) {
      # Also guess backwards, from Non-US to Debian:
      uri = $1;
      sub(/[-\/]non-[uU][sS]\//, "/", uri);
      if (uri != $1) jigdo("Debian="uri, nonUsCountry);
    }
  }
}
