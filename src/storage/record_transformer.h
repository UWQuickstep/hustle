
#include <assert.h>

typedef signed char i8;
typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned short u16;
typedef short i16;
typedef long long int i64;
typedef unsigned long long int u64;

#define SQLITE_MAX_U32 ((((u64)1) << 32) - 1)

u8 sqlite3GetVarint32(const unsigned char *p, u32 *v);

#define getVarint32(A, B)                       \
  (u8)((*(A) < (u8)0x80) ? ((B) = (u32) * (A)), \
       1 : sqlite3GetVarint32((A), (u32 *)&(B)))

#define SLOT_2_0 0x001fc07f
#define SLOT_4_2_0 0xf01fc07f


/*
** The sizes for serial types less than 128
*/
static const u8 smallTypeSizes[] = {
        /*  0   1   2   3   4   5   6   7   8   9 */   
/*   0 */   0,  1,  2,  3,  4,  6,  8,  8,  0,  0,
/*  10 */   0,  0,  0,  0,  1,  1,  2,  2,  3,  3,
/*  20 */   4,  4,  5,  5,  6,  6,  7,  7,  8,  8,
/*  30 */   9,  9, 10, 10, 11, 11, 12, 12, 13, 13,
/*  40 */  14, 14, 15, 15, 16, 16, 17, 17, 18, 18,
/*  50 */  19, 19, 20, 20, 21, 21, 22, 22, 23, 23,
/*  60 */  24, 24, 25, 25, 26, 26, 27, 27, 28, 28,
/*  70 */  29, 29, 30, 30, 31, 31, 32, 32, 33, 33,
/*  80 */  34, 34, 35, 35, 36, 36, 37, 37, 38, 38,
/*  90 */  39, 39, 40, 40, 41, 41, 42, 42, 43, 43,
/* 100 */  44, 44, 45, 45, 46, 46, 47, 47, 48, 48,
/* 110 */  49, 49, 50, 50, 51, 51, 52, 52, 53, 53,
/* 120 */  54, 54, 55, 55, 56, 56, 57, 57
};

/*
** Return the length of the data corresponding to the supplied serial-type.
*/
u32 serialTypeLen(u32 serial_type){
  if( serial_type>=128 ){
    return (serial_type-12)/2;
  }else{
    assert( serial_type<12 
            || smallTypeSizes[serial_type]==(serial_type - 12)/2 );
    return smallTypeSizes[serial_type];
  }
}

/*
** Read a 64-bit variable-length integer from memory starting at p[0].
** Return the number of bytes read.  The value is stored in *v.
*/
u8 sqlite3GetVarint(const unsigned char *p, u64 *v) {
  u32 a, b, s;

  if (((signed char *)p)[0] >= 0) {
    *v = *p;
    return 1;
  }
  if (((signed char *)p)[1] >= 0) {
    *v = ((u32)(p[0] & 0x7f) << 7) | p[1];
    return 2;
  }

  /* Verify that constants are precomputed correctly */
  assert(SLOT_2_0 == ((0x7f << 14) | (0x7f)));
  assert(SLOT_4_2_0 == ((0xfU << 28) | (0x7f << 14) | (0x7f)));

  a = ((u32)p[0]) << 14;
  b = p[1];
  p += 2;
  a |= *p;
  /* a: p0<<14 | p2 (unmasked) */
  if (!(a & 0x80)) {
    a &= SLOT_2_0;
    b &= 0x7f;
    b = b << 7;
    a |= b;
    *v = a;
    return 3;
  }

  /* CSE1 from below */
  a &= SLOT_2_0;
  p++;
  b = b << 14;
  b |= *p;
  /* b: p1<<14 | p3 (unmasked) */
  if (!(b & 0x80)) {
    b &= SLOT_2_0;
    /* moved CSE1 up */
    /* a &= (0x7f<<14)|(0x7f); */
    a = a << 7;
    a |= b;
    *v = a;
    return 4;
  }

  /* a: p0<<14 | p2 (masked) */
  /* b: p1<<14 | p3 (unmasked) */
  /* 1:save off p0<<21 | p1<<14 | p2<<7 | p3 (masked) */
  /* moved CSE1 up */
  /* a &= (0x7f<<14)|(0x7f); */
  b &= SLOT_2_0;
  s = a;
  /* s: p0<<14 | p2 (masked) */

  p++;
  a = a << 14;
  a |= *p;
  /* a: p0<<28 | p2<<14 | p4 (unmasked) */
  if (!(a & 0x80)) {
    /* we can skip these cause they were (effectively) done above
    ** while calculating s */
    /* a &= (0x7f<<28)|(0x7f<<14)|(0x7f); */
    /* b &= (0x7f<<14)|(0x7f); */
    b = b << 7;
    a |= b;
    s = s >> 18;
    *v = ((u64)s) << 32 | a;
    return 5;
  }

  /* 2:save off p0<<21 | p1<<14 | p2<<7 | p3 (masked) */
  s = s << 7;
  s |= b;
  /* s: p0<<21 | p1<<14 | p2<<7 | p3 (masked) */

  p++;
  b = b << 14;
  b |= *p;
  /* b: p1<<28 | p3<<14 | p5 (unmasked) */
  if (!(b & 0x80)) {
    /* we can skip this cause it was (effectively) done above in calc'ing s */
    /* b &= (0x7f<<28)|(0x7f<<14)|(0x7f); */
    a &= SLOT_2_0;
    a = a << 7;
    a |= b;
    s = s >> 18;
    *v = ((u64)s) << 32 | a;
    return 6;
  }

  p++;
  a = a << 14;
  a |= *p;
  /* a: p2<<28 | p4<<14 | p6 (unmasked) */
  if (!(a & 0x80)) {
    a &= SLOT_4_2_0;
    b &= SLOT_2_0;
    b = b << 7;
    a |= b;
    s = s >> 11;
    *v = ((u64)s) << 32 | a;
    return 7;
  }

  /* CSE2 from below */
  a &= SLOT_2_0;
  p++;
  b = b << 14;
  b |= *p;
  /* b: p3<<28 | p5<<14 | p7 (unmasked) */
  if (!(b & 0x80)) {
    b &= SLOT_4_2_0;
    /* moved CSE2 up */
    /* a &= (0x7f<<14)|(0x7f); */
    a = a << 7;
    a |= b;
    s = s >> 4;
    *v = ((u64)s) << 32 | a;
    return 8;
  }

  p++;
  a = a << 15;
  a |= *p;
  /* a: p4<<29 | p6<<15 | p8 (unmasked) */

  /* moved CSE2 up */
  /* a &= (0x7f<<29)|(0x7f<<15)|(0xff); */
  b &= SLOT_2_0;
  b = b << 8;
  a |= b;

  s = s << 4;
  b = p[-4];
  b &= 0x7f;
  b = b >> 3;
  s |= b;

  *v = ((u64)s) << 32 | a;

  return 9;
}

/*
** Read a 32-bit variable-length integer from memory starting at p[0].
** Return the number of bytes read.  The value is stored in *v.
**
** If the varint stored in p[0] is larger than can fit in a 32-bit unsigned
** integer, then set *v to 0xffffffff.
**
** A MACRO version, getVarint32, is provided which inlines the
** single-byte case.  All code should use the MACRO version as
** this function assumes the single-byte case has already been handled.
*/
u8 sqlite3GetVarint32(const unsigned char *p, u32 *v) {
  u32 a, b;

  /* The 1-byte case.  Overwhelmingly the most common.  Handled inline
  ** by the getVarin32() macro */
  a = *p;
  /* a: p0 (unmasked) */
#ifndef getVarint32
  if (!(a & 0x80)) {
    /* Values between 0 and 127 */
    *v = a;
    return 1;
  }
#endif

  /* The 2-byte case */
  p++;
  b = *p;
  /* b: p1 (unmasked) */
  if (!(b & 0x80)) {
    /* Values between 128 and 16383 */
    a &= 0x7f;
    a = a << 7;
    *v = a | b;
    return 2;
  }

  /* The 3-byte case */
  p++;
  a = a << 14;
  a |= *p;
  /* a: p0<<14 | p2 (unmasked) */
  if (!(a & 0x80)) {
    /* Values between 16384 and 2097151 */
    a &= (0x7f << 14) | (0x7f);
    b &= 0x7f;
    b = b << 7;
    *v = a | b;
    return 3;
  }

  /* A 32-bit varint is used to store size information in btrees.
  ** Objects are rarely larger than 2MiB limit of a 3-byte varint.
  ** A 3-byte varint is sufficient, for example, to record the size
  ** of a 1048569-byte BLOB or string.
  **
  ** We only unroll the first 1-, 2-, and 3- byte cases.  The very
  ** rare larger cases can be handled by the slower 64-bit varint
  ** routine.
  */
#if 1
  {
    u64 v64;
    u8 n;

    p -= 2;
    n = sqlite3GetVarint(p, &v64);
    assert(n > 3 && n <= 9);
    if ((v64 & SQLITE_MAX_U32) != v64) {
      *v = 0xffffffff;
    } else {
      *v = (u32)v64;
    }
    return n;
  }

#else
  /* For following code (kept for historical record only) shows an
  ** unrolling for the 3- and 4-byte varint cases.  This code is
  ** slightly faster, but it is also larger and much harder to test.
  */
  p++;
  b = b << 14;
  b |= *p;
  /* b: p1<<14 | p3 (unmasked) */
  if (!(b & 0x80)) {
    /* Values between 2097152 and 268435455 */
    b &= (0x7f << 14) | (0x7f);
    a &= (0x7f << 14) | (0x7f);
    a = a << 7;
    *v = a | b;
    return 4;
  }

  p++;
  a = a << 14;
  a |= *p;
  /* a: p0<<28 | p2<<14 | p4 (unmasked) */
  if (!(a & 0x80)) {
    /* Values  between 268435456 and 34359738367 */
    a &= SLOT_4_2_0;
    b &= SLOT_4_2_0;
    b = b << 7;
    *v = a | b;
    return 5;
  }

  /* We can only reach this point when reading a corrupt database
  ** file.  In that case we are not in any hurry.  Use the (relatively
  ** slow) general-purpose sqlite3GetVarint() routine to extract the
  ** value. */
  {
    u64 v64;
    u8 n;

    p -= 4;
    n = sqlite3GetVarint(p, &v64);
    assert(n > 5 && n <= 9);
    *v = (u32)v64;
    return n;
  }
#endif
}