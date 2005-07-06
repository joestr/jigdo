// Taken from glibc 2.1.3, updated from 2.2.5

/* md5.c - Functions to compute MD5 message digest of files or memory blocks
   according to the definition of MD5 in RFC 1321 from April 1992.
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

/* Written by Ulrich Drepper <drepper@gnu.ai.mit.edu>, 1995.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <sys/types.h>

#if STDC_HEADERS || defined _LIBC
# include <stdlib.h>
# include <string.h>
#else
# ifndef HAVE_MEMCPY
#  define memcpy(d, s, n) bcopy ((s), (d), (n))
# endif
#endif

/* [RA] #include "md5.h" */
#include <md5sum.hh>
#include <glibc-md5.hh>

#ifdef _LIBC
# include <endian.h>
# if __BYTE_ORDER == __BIG_ENDIAN
#  define WORDS_BIGENDIAN 1
# endif
/* We need to keep the namespace clean so define the MD5 function
   protected using leading __ .  */
/* [RA] # define md5_init_ctx __md5_init_ctx */
/* [RA] # define md5_process_block __md5_process_block */
/* [RA] # define md5_process_bytes __md5_process_bytes */
/* [RA] # define md5_finish_ctx __md5_finish_ctx */
/* [RA] # define md5_read_ctx __md5_read_ctx */
/* [RA] # define md5_stream __md5_stream */
/* [RA] # define md5_buffer __md5_buffer */
#endif

#ifdef WORDS_BIGENDIAN
# define SWAP(n)                                                        \
    (((n) << 24) | (((n) & 0xff00) << 8) | (((n) >> 8) & 0xff00) | ((n) >> 24))
#else
# define SWAP(n) (n)
#endif


/* This array contains the bytes used to pad the buffer to the next
   64-byte boundary.  (RFC 1321, 3.1: Step 1)  */
static const unsigned char fillbuf[64] = { 0x80, 0 /* , 0, 0, ...  */ };


/* Initialize structure containing state of computation.
   (RFC 1321, 3.3: Step 3)  */
void MD5Sum::md5_init_ctx (md5_ctx *ctx)
{
  ctx->A = 0x67452301;
  ctx->B = 0xefcdab89;
  ctx->C = 0x98badcfe;
  ctx->D = 0x10325476;

  ctx->total[0] = ctx->total[1] = 0;
  ctx->buflen = 0;
}

/* Put result from CTX in first 16 bytes following RESBUF.  The result
   must be in little endian byte order.

   IMPORTANT: On some systems it is required that RESBUF is correctly
   aligned for a 32 bits value.  */
byte* MD5Sum::md5_read_ctx(const md5_ctx *ctx, byte* resbuf)
{
  ((uint32 *) resbuf)[0] = SWAP (ctx->A);
  ((uint32 *) resbuf)[1] = SWAP (ctx->B);
  ((uint32 *) resbuf)[2] = SWAP (ctx->C);
  ((uint32 *) resbuf)[3] = SWAP (ctx->D);

  return resbuf;
}

/* Process the remaining bytes in the internal buffer and the usual
   prolog according to the standard and write the result to RESBUF.

   IMPORTANT: On some systems it is required that RESBUF is correctly
   aligned for a 32 bits value.  */
byte* MD5Sum::md5_finish_ctx(struct md5_ctx* ctx, byte* resbuf)
{
  /* Take yet unprocessed bytes into account.  */
  uint32 bytes = ctx->buflen;
  size_t pad;

  /* Now count remaining bytes.  */
  ctx->total[0] += bytes;
  if (ctx->total[0] < bytes)
    ++ctx->total[1];

  pad = bytes >= 56 ? 64 + 56 - bytes : 56 - bytes;
  memcpy (&ctx->buffer[bytes], fillbuf, pad);

  /* Put the 64-bit file length in *bits* at the end of the buffer.  */
  *(uint32 *) &ctx->buffer[bytes + pad] = SWAP (ctx->total[0] << 3);
  *(uint32 *) &ctx->buffer[bytes + pad + 4] = SWAP ((ctx->total[1] << 3) |
                                                    (ctx->total[0] >> 29));

  /* Process last bytes.  */
  md5_process_block (ctx->buffer, bytes + pad + 8, ctx);

  return md5_read_ctx (ctx, resbuf);
}

/* Compute MD5 message digest for bytes read from STREAM.  The
   resulting message digest number will be written into the 16 bytes
   beginning at RESBLOCK.  */
//[RA] int
//[RA] md5_stream (stream, resblock)
//[RA]      FILE *stream;
//[RA]      void *resblock;
//[RA] {
//[RA]   /* Important: BLOCKSIZE must be a multiple of 64.  */
//[RA] #define BLOCKSIZE 4096
//[RA]   md5_ctx ctx; // [RA]
//[RA]   char buffer[BLOCKSIZE + 72];
//[RA]   size_t sum;
//[RA]
//[RA]   /* Initialize the computation context.  */
//[RA]   md5_init_ctx (&ctx);
//[RA]
//[RA]   /* Iterate over full file contents.  */
//[RA]   while (1)
//[RA]     {
//[RA]       /* We read the file in blocks of BLOCKSIZE bytes.  One call of the
//[RA]   computation function processes the whole buffer so that with the
//[RA]   next round of the loop another block can be read.  */
//[RA]       size_t n;
//[RA]       sum = 0;
//[RA]
//[RA]       /* Read block.  Take care for partial reads.  */
//[RA]       do
//[RA]  {
//[RA]    n = fread (buffer + sum, 1, BLOCKSIZE - sum, stream);
//[RA]
//[RA]    sum += n;
//[RA]  }
//[RA]       while (sum < BLOCKSIZE && n != 0);
//[RA]       if (n == 0 && ferror (stream))
//[RA]         return 1;
//[RA]
//[RA]       /* If end of file is reached, end the loop.  */
//[RA]       if (n == 0)
//[RA]  break;
//[RA]
//[RA]       /* Process buffer with BLOCKSIZE bytes.  Note that
//[RA]                  BLOCKSIZE % 64 == 0
//[RA]        */
//[RA]       md5_process_block (buffer, BLOCKSIZE, &ctx);
//[RA]     }
//[RA]
//[RA]   /* Add the last bytes if necessary.  */
//[RA]   if (sum > 0)
//[RA]     md5_process_bytes (buffer, sum, &ctx);
//[RA]
//[RA]   /* Construct result in desired memory.  */
//[RA]   md5_finish_ctx (&ctx, resblock);
//[RA]   return 0;
//[RA] }

//[RA] /* Compute MD5 message digest for LEN bytes beginning at BUFFER.  The
//[RA]    result is always in little endian byte order, so that a byte-wise
//[RA]    output yields to the wanted ASCII representation of the message
//[RA]    digest.  */
//[RA] void *
//[RA] md5_buffer (buffer, len, resblock)
//[RA]      const char *buffer;
//[RA]      size_t len;
//[RA]      void *resblock;
//[RA] {
//[RA]   md5_ctx ctx; // RA
//[RA]
//[RA]   /* Initialize the computation context.  */
//[RA]   md5_init_ctx (&ctx);
//[RA]
//[RA]   /* Process whole buffer but last len % 64 bytes.  */
//[RA]   md5_process_bytes (buffer, len, &ctx);
//[RA]
//[RA]   /* Put result in desired memory area.  */
//[RA]   return md5_finish_ctx (&ctx, resblock);
//[RA] }


void MD5Sum::md5_process_bytes(const void* buffer, size_t len,
                               struct md5_ctx* ctx)
{
  /* When we already have some bits in our internal buffer concatenate
     both inputs first.  */
  if (ctx->buflen != 0)
    {
      size_t left_over = ctx->buflen;
      size_t add = 128 - left_over > len ? len : 128 - left_over;

      memcpy (&ctx->buffer[left_over], buffer, add);
      ctx->buflen += add;

      if (ctx->buflen > 64)
        {
          md5_process_block (ctx->buffer, ctx->buflen & ~63, ctx);

          ctx->buflen &= 63;
          /* The regions in the following copy operation cannot overlap.  */
          memcpy (ctx->buffer, &ctx->buffer[(left_over + add) & ~63],
                  ctx->buflen);
        }

      buffer = (const char *) buffer + add;
      len -= add;
    }

   /* Process available complete blocks.  */
   if (len >= 64)
     {
#if !_STRING_ARCH_unaligned
/* To check alignment gcc has an appropriate operator.  Other
   compilers don't.  */
# if __GNUC__ >= 2
#  define UNALIGNED_P(p) (((md5_uintptr) p) % __alignof__ (uint32) != 0)
# else
#  define UNALIGNED_P(p) (((md5_uintptr) p) % sizeof (uint32) != 0)
# endif
       if (UNALIGNED_P (buffer))
         while (len > 64)
           {
             md5_process_block (memcpy (ctx->buffer, buffer, 64), 64, ctx);
             buffer = (const char *) buffer + 64;
             len -= 64;
           }
       else
#endif
         {
           md5_process_block (buffer, len & ~63, ctx);
           buffer = (const char *) buffer + (len & ~63);
           len &= 63;
         }
    }

   /* Move remaining bytes in internal buffer.  */
   if (len > 0)
     {
       size_t left_over = ctx->buflen;

       memcpy (&ctx->buffer[left_over], buffer, len);
       left_over += len;
       if (left_over >= 64)
         {
           md5_process_block (ctx->buffer, 64, ctx);
           left_over -= 64;
           memcpy (ctx->buffer, &ctx->buffer[64], left_over);
         }
       ctx->buflen = left_over;
     }
 }


/* These are the four functions used in the four steps of the MD5 algorithm
   and defined in the RFC 1321.  The first function is a little bit optimized
   (as found in Colin Plumbs public domain implementation).  */
/* #define FF(b, c, d) ((b & c) | (~b & d)) */
#define FF(b, c, d) (d ^ (b & (c ^ d)))
#define FG(b, c, d) FF (d, b, c)
#define FH(b, c, d) (b ^ c ^ d)
#define FI(b, c, d) (c ^ (b | ~d))

/* Process LEN bytes of BUFFER, accumulating context into CTX.
   It is assumed that LEN % 64 == 0.  */

void MD5Sum::md5_process_block(const void* buffer, size_t len, md5_ctx* ctx)
{
  uint32 correct_words[16];
  const uint32 *words = (uint32*)(buffer);
  size_t nwords = len / sizeof (uint32);
  const uint32 *endp = words + nwords;
  uint32 A = ctx->A;
  uint32 B = ctx->B;
  uint32 C = ctx->C;
  uint32 D = ctx->D;

  /* First increment the byte count.  RFC 1321 specifies the possible
     length of the file up to 2^64 bits.  Here we only compute the
     number of bytes.  Do a double word increment.  */
  ctx->total[0] += len;
  if (ctx->total[0] < len)
    ++ctx->total[1];

  /* Process all bytes in the buffer with 64 bytes in each round of
     the loop.  */
  while (words < endp)
    {
      uint32 *cwp = correct_words;
      uint32 A_save = A;
      uint32 B_save = B;
      uint32 C_save = C;
      uint32 D_save = D;

      /* First round: using the given function, the context and a constant
         the next context is computed.  Because the algorithms processing
         unit is a 32-bit word and it is determined to work on words in
         little endian byte order we perhaps have to change the byte order
         before the computation.  To reduce the work for the next steps
         we store the swapped words in the array CORRECT_WORDS.  */

#define OP(a, b, c, d, s, T)                                            \
      do                                                                \
        {                                                               \
          a += FF (b, c, d) + (*cwp++ = SWAP (*words)) + T;             \
          ++words;                                                      \
          CYCLIC (a, s);                                                \
          a += b;                                                       \
        }                                                               \
      while (0)

      /* It is unfortunate that C does not provide an operator for
         cyclic rotation.  Hope the C compiler is smart enough.  */
#define CYCLIC(w, s) (w = (w << s) | (w >> (32 - s)))

      /* Before we start, one word to the strange constants.
         They are defined in RFC 1321 as

         T[i] = (int) (4294967296.0 * fabs (sin (i))), i=1..64
       */

      /* Round 1.  */
      OP (A, B, C, D,  7, 0xd76aa478);
      OP (D, A, B, C, 12, 0xe8c7b756);
      OP (C, D, A, B, 17, 0x242070db);
      OP (B, C, D, A, 22, 0xc1bdceee);
      OP (A, B, C, D,  7, 0xf57c0faf);
      OP (D, A, B, C, 12, 0x4787c62a);
      OP (C, D, A, B, 17, 0xa8304613);
      OP (B, C, D, A, 22, 0xfd469501);
      OP (A, B, C, D,  7, 0x698098d8);
      OP (D, A, B, C, 12, 0x8b44f7af);
      OP (C, D, A, B, 17, 0xffff5bb1);
      OP (B, C, D, A, 22, 0x895cd7be);
      OP (A, B, C, D,  7, 0x6b901122);
      OP (D, A, B, C, 12, 0xfd987193);
      OP (C, D, A, B, 17, 0xa679438e);
      OP (B, C, D, A, 22, 0x49b40821);

      /* For the second to fourth round we have the possibly swapped words
         in CORRECT_WORDS.  Redefine the macro to take an additional first
         argument specifying the function to use.  */
#undef OP
#define OP(f, a, b, c, d, k, s, T)                                      \
      do                                                                \
        {                                                               \
          a += f (b, c, d) + correct_words[k] + T;                      \
          CYCLIC (a, s);                                                \
          a += b;                                                       \
        }                                                               \
      while (0)

      /* Round 2.  */
      OP (FG, A, B, C, D,  1,  5, 0xf61e2562);
      OP (FG, D, A, B, C,  6,  9, 0xc040b340);
      OP (FG, C, D, A, B, 11, 14, 0x265e5a51);
      OP (FG, B, C, D, A,  0, 20, 0xe9b6c7aa);
      OP (FG, A, B, C, D,  5,  5, 0xd62f105d);
      OP (FG, D, A, B, C, 10,  9, 0x02441453);
      OP (FG, C, D, A, B, 15, 14, 0xd8a1e681);
      OP (FG, B, C, D, A,  4, 20, 0xe7d3fbc8);
      OP (FG, A, B, C, D,  9,  5, 0x21e1cde6);
      OP (FG, D, A, B, C, 14,  9, 0xc33707d6);
      OP (FG, C, D, A, B,  3, 14, 0xf4d50d87);
      OP (FG, B, C, D, A,  8, 20, 0x455a14ed);
      OP (FG, A, B, C, D, 13,  5, 0xa9e3e905);
      OP (FG, D, A, B, C,  2,  9, 0xfcefa3f8);
      OP (FG, C, D, A, B,  7, 14, 0x676f02d9);
      OP (FG, B, C, D, A, 12, 20, 0x8d2a4c8a);

      /* Round 3.  */
      OP (FH, A, B, C, D,  5,  4, 0xfffa3942);
      OP (FH, D, A, B, C,  8, 11, 0x8771f681);
      OP (FH, C, D, A, B, 11, 16, 0x6d9d6122);
      OP (FH, B, C, D, A, 14, 23, 0xfde5380c);
      OP (FH, A, B, C, D,  1,  4, 0xa4beea44);
      OP (FH, D, A, B, C,  4, 11, 0x4bdecfa9);
      OP (FH, C, D, A, B,  7, 16, 0xf6bb4b60);
      OP (FH, B, C, D, A, 10, 23, 0xbebfbc70);
      OP (FH, A, B, C, D, 13,  4, 0x289b7ec6);
      OP (FH, D, A, B, C,  0, 11, 0xeaa127fa);
      OP (FH, C, D, A, B,  3, 16, 0xd4ef3085);
      OP (FH, B, C, D, A,  6, 23, 0x04881d05);
      OP (FH, A, B, C, D,  9,  4, 0xd9d4d039);
      OP (FH, D, A, B, C, 12, 11, 0xe6db99e5);
      OP (FH, C, D, A, B, 15, 16, 0x1fa27cf8);
      OP (FH, B, C, D, A,  2, 23, 0xc4ac5665);

      /* Round 4.  */
      OP (FI, A, B, C, D,  0,  6, 0xf4292244);
      OP (FI, D, A, B, C,  7, 10, 0x432aff97);
      OP (FI, C, D, A, B, 14, 15, 0xab9423a7);
      OP (FI, B, C, D, A,  5, 21, 0xfc93a039);
      OP (FI, A, B, C, D, 12,  6, 0x655b59c3);
      OP (FI, D, A, B, C,  3, 10, 0x8f0ccc92);
      OP (FI, C, D, A, B, 10, 15, 0xffeff47d);
      OP (FI, B, C, D, A,  1, 21, 0x85845dd1);
      OP (FI, A, B, C, D,  8,  6, 0x6fa87e4f);
      OP (FI, D, A, B, C, 15, 10, 0xfe2ce6e0);
      OP (FI, C, D, A, B,  6, 15, 0xa3014314);
      OP (FI, B, C, D, A, 13, 21, 0x4e0811a1);
      OP (FI, A, B, C, D,  4,  6, 0xf7537e82);
      OP (FI, D, A, B, C, 11, 10, 0xbd3af235);
      OP (FI, C, D, A, B,  2, 15, 0x2ad7d2bb);
      OP (FI, B, C, D, A,  9, 21, 0xeb86d391);

      /* Add the starting values of the context.  */
      A += A_save;
      B += B_save;
      C += C_save;
      D += D_save;
    }

  /* Put checksum in context given as argument.  */
  ctx->A = A;
  ctx->B = B;
  ctx->C = C;
  ctx->D = D;
}
