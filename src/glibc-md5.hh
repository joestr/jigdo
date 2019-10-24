/* Declaration of functions and data types used for MD5 sum computing
   library functions.
   Copyright (C) 1995, 1996, 1997, 1999 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#ifndef _MD5_H
#define _MD5_H 1

#include <stdio.h>

#if defined HAVE_LIMITS_H || _LIBC
# include <limits.h>
#endif

/* The following contortions are an attempt to use the C preprocessor
   to determine an unsigned integral type that is 32 bits wide.  An
   alternative approach is to use autoconf's AC_CHECK_SIZEOF macro, but
   doing that would require that the configure script compile and *run*
   the resulting executable.  Locally running cross-compiled executables
   is usually not possible.  */

//[RA] #ifdef _LIBC
//[RA] # include <sys/types.h>
//[RA] typedef u_int32_t uint32;
//[RA] #else
//[RA] # if defined __STDC__ && __STDC__
//[RA] #  define UINT_MAX_32_BITS 4294967295U
//[RA] # else
//[RA] #  define UINT_MAX_32_BITS 0xFFFFFFFF
//[RA] # endif
//[RA]
//[RA] /* If UINT_MAX isn't defined, assume it's a 32-bit type.
//[RA]    This should be valid for all systems GNU cares about because
//[RA]    that doesn't include 16-bit systems, and only modern systems
//[RA]    (that certainly have <limits.h>) have 64+-bit integral types.  */
//[RA]
//[RA] # ifndef UINT_MAX
//[RA] #  define UINT_MAX UINT_MAX_32_BITS
//[RA] # endif
//[RA]
//[RA] # if UINT_MAX == UINT_MAX_32_BITS
//[RA]    typedef unsigned int uint32;
//[RA] # else
//[RA] #  if USHRT_MAX == UINT_MAX_32_BITS
//[RA]     typedef unsigned short uint32;
//[RA] #  else
//[RA] #   if ULONG_MAX == UINT_MAX_32_BITS
//[RA]      typedef unsigned long uint32;
//[RA] #   else
//[RA]      /* The following line is intended to evoke an error.
//[RA]         Using #error is not portable enough.  */
//[RA]      "Cannot determine unsigned 32-bit data type."
//[RA] #   endif
//[RA] #  endif
//[RA] # endif
//[RA] #endif

#undef __P
#if defined (__STDC__) && __STDC__
# define __P(x) x
#else
# define __P(x) ()
#endif

/* Structure to save state of computation between the single steps.  */
/* [RA] moved this to md5sum.hh */
     /* [RA] struct md5_ctx */
     /* [RA] { */
     /* [RA]   uint32 A; */
     /* [RA]   uint32 B; */
     /* [RA]   uint32 C; */
     /* [RA]   uint32 D; */
     /* [RA]  */
     /* [RA]   uint32 total[2]; */
     /* [RA]   uint32 buflen; */
     /* [RA]   char buffer[128]; */
     /* [RA] }; */

/*
 * The following three functions are build up the low level used in
 * the functions `md5_stream' and `md5_buffer'.
 */

/* Initialize structure containing state of computation.
   (RFC 1321, 3.3: Step 3)  */
extern void md5_init_ctx __P ((struct md5_ctx *ctx));
/* [RA] extern void __md5_init_ctx __P ((struct md5_ctx *ctx)); */

/* Starting with the result of former calls of this function (or the
   initialization function update the context for the next LEN bytes
   starting at BUFFER.
   It is necessary that LEN is a multiple of 64!!! */
extern void md5_process_block __P ((const void *buffer, size_t len,
                                      struct md5_ctx *ctx));
/* [RA] extern void __md5_process_block __P ((const void *buffer, size_t len, */
/* [RA]                                       struct md5_ctx *ctx)); */

/* Starting with the result of former calls of this function (or the
   initialization function update the context for the next LEN bytes
   starting at BUFFER.
   It is NOT required that LEN is a multiple of 64.  */
extern void md5_process_bytes __P ((const void *buffer, size_t len,
                                      struct md5_ctx *ctx));
/* [RA] extern void __md5_process_bytes __P ((const void *buffer, size_t len, */
/* [RA]                                       struct md5_ctx *ctx)); */

/* Process the remaining bytes in the buffer and put result from CTX
   in first 16 bytes following RESBUF.  The result is always in little
   endian byte order, so that a byte-wise output yields to the wanted
   ASCII representation of the message digest.

   IMPORTANT: On some systems it is required that RESBUF is correctly
   aligned for a 32 bits value.  */
extern void *md5_finish_ctx __P ((struct md5_ctx *ctx, void *resbuf));
/* [RA] extern void *__md5_finish_ctx __P ((struct md5_ctx *ctx, void *resbuf)); */


/* Put result from CTX in first 16 bytes following RESBUF.  The result is
   always in little endian byte order, so that a byte-wise output yields
   to the wanted ASCII representation of the message digest.

   IMPORTANT: On some systems it is required that RESBUF is correctly
   aligned for a 32 bits value.  */
extern void *md5_read_ctx __P ((const struct md5_ctx *ctx, void *resbuf));
/* [RA] extern void *__md5_read_ctx __P ((const struct md5_ctx *ctx, void *resbuf)); */


/* Compute MD5 message digest for bytes read from STREAM.  The
   resulting message digest number will be written into the 16 bytes
   beginning at RESBLOCK.  */
/* [RA] extern int __md5_stream __P ((FILE *stream, void *resblock)); */

/* Compute MD5 message digest for LEN bytes beginning at BUFFER.  The
   result is always in little endian byte order, so that a byte-wise
   output yields to the wanted ASCII representation of the message
   digest.  */
/* [RA] extern void *__md5_buffer __P ((const char *buffer, size_t len, */
/* [RA]                                 void *resblock)); */

#endif /* md5.h */
