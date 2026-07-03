/* ==========================================================================
 * Achilles-Search | include/common/types.h
 * ==========================================================================
 * Portable type aliases for the entire project.
 *
 * WHY THIS FILE EXISTS:
 *   C's built-in integer types (int, long, unsigned long) have platform-
 *   dependent sizes. On Windows x64, `long` is 4 bytes; on Linux x64, it's
 *   8 bytes. This causes subtle, hard-to-find bugs when you serialize data,
 *   compute hashes, or interface with OS APIs.
 *
 *   By aliasing over <stdint.h> fixed-width types, we:
 *   1. Make intent explicit: u32 means "exactly 32 unsigned bits"
 *   2. Prevent platform surprises
 *   3. Reduce visual noise (uint32_t -> u32)
 *
 * USAGE:
 *   #include "common/types.h"  // Always include this first
 *   u32 count = 0;
 *   f64 elapsed = 3.14;
 * ========================================================================== */

#ifndef ACH_TYPES_H
#define ACH_TYPES_H

/* ---- Standard Headers --------------------------------------------------- */
#include <stdint.h>     /* Fixed-width integers: uint8_t, int32_t, etc. */
#include <stdbool.h>    /* bool, true, false (C99+) */
#include <stddef.h>     /* size_t, NULL, ptrdiff_t */

/* ---- Unsigned Integer Aliases ------------------------------------------- */
typedef uint8_t   u8;       /* [0, 255]                          - byte       */
typedef uint16_t  u16;      /* [0, 65535]                        - short      */
typedef uint32_t  u32;      /* [0, 4,294,967,295]                - word       */
typedef uint64_t  u64;      /* [0, 18,446,744,073,709,551,615]   - dword      */

/* ---- Signed Integer Aliases --------------------------------------------- */
typedef int8_t    i8;       /* [-128, 127]                                    */
typedef int16_t   i16;      /* [-32768, 32767]                                */
typedef int32_t   i32;      /* [-2^31, 2^31-1]                                */
typedef int64_t   i64;      /* [-2^63, 2^63-1]                                */

/* ---- Floating Point Aliases --------------------------------------------- */
typedef float     f32;      /* IEEE 754 single-precision (7 digits)           */
typedef double    f64;      /* IEEE 754 double-precision (15 digits)          */

/* ---- Size / Index Type -------------------------------------------------- */
/* usize matches pointer width: 4 bytes on x86, 8 bytes on x64.
 * Use for array indices, byte counts, and anything measuring memory. */
typedef size_t    usize;

/* ---- Byte Type ---------------------------------------------------------- */
/* Explicit "raw byte" type for buffers, serialization, memory mapping.
 * Distinct from u8 in semantic intent (though identical in representation). */
typedef uint8_t   byte;

#endif /* ACH_TYPES_H */
