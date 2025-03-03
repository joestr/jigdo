/* $Id: nocopy.hh,v 1.2 2004/09/09 23:50:22 atterer Exp $ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/�|  Richard Atterer     |  atterer.org
  � '` �
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

*/

/** @file
    A class which prevents derived classes from being copied. */

#ifndef NOCOPY_HH
#define NOCOPY_HH

/** To be used as a base class only - prevents that the derived class can be
    copied. It would be equivalent to add a private copy ctor and a private
    assignment operator to the derived class, but deriving from NoCopy saves
    typing and looks cleaner. */
class NoCopy {
protected:
  NoCopy() { }
  ~NoCopy() { }
private:
  NoCopy(const NoCopy&);
  const NoCopy& operator=(const NoCopy&);
};

#endif
