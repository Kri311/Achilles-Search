/* ==========================================================================
 * Achilles-Search | include/common/macros.h
 * ==========================================================================
 * Utility macros used across the entire project.
 *
 * WHY THIS FILE EXISTS:
 *   Certain patterns appear hundreds of times in any C project:
 *   - Computing array length
 *   - Min/max without double-evaluation bugs
 *   - Marking parameters as intentionally unused
 *   - Alignment and rounding
 *   - Compile-time assertions
 *
 *   Centralizing these prevents copy-paste bugs and keeps code concise.
 *
 * NAMING:
 *   All macros are prefixed with ACH_ to avoid collisions with Windows
 *   headers, which define their own min, max, ARRAYSIZE, etc.
 *
 * WARNING:
 *   Macros have no type safety. Use with care. Prefer inline functions
 *   where possible (C99+). Macros are used here only for things that
 *   cannot be done with functions (e.g., __FILE__, _Static_assert,
 *   sizeof on arrays).
 * ========================================================================== */

#ifndef ACH_MACROS_H
#define ACH_MACROS_H

#include "common/types.h"

/* ---- Array Length --------------------------------------------------------
 * Returns the number of elements in a statically allocated array.
 *
 * CRITICAL: This ONLY works on actual arrays, NOT pointers.
 *   int arr[10];       ACH_ARRAY_LENGTH(arr)  -> 10  (correct)
 *   int *p = arr;      ACH_ARRAY_LENGTH(p)    -> ??  (WRONG! gives sizeof(ptr)/sizeof(int))
 *
 * There is no way in standard C to make this fail at compile time for
 * pointers (C++ can do it with templates). Be disciplined about usage.
 * ------------------------------------------------------------------------- */
#define ACH_ARRAY_LENGTH(arr) (sizeof(arr) / sizeof((arr)[0]))

/* ---- Min / Max ----------------------------------------------------------
 * PROBLEM with naive #define MIN(a,b) ((a) < (b) ? (a) : (b)):
 *   MIN(x++, y) expands to ((x++) < (y) ? (x++) : (y))
 *   x gets incremented TWICE if x < y.
 *
 * SOLUTION: Use statement expressions (GCC/Clang) or just document the
 * limitation. MSVC doesn't support statement expressions in C mode, so
 * we use the simple version with a clear warning.
 *
 * For hot loops where double evaluation matters, use a local variable:
 *   int a = compute_a();
 *   int b = compute_b();
 *   int result = ACH_MIN(a, b);  // safe: no side effects
 * ------------------------------------------------------------------------- */
#define ACH_MIN(a, b) (((a) < (b)) ? (a) : (b))
#define ACH_MAX(a, b) (((a) > (b)) ? (a) : (b))

/* ---- Clamping ----------------------------------------------------------- */
#define ACH_CLAMP(val, lo, hi) (ACH_MIN(ACH_MAX((val), (lo)), (hi)))

/* ---- Unused Parameters --------------------------------------------------
 * Suppresses "unreferenced parameter" warnings (MSVC C4100).
 * Use when a function signature requires a parameter (callback, interface)
 * but the implementation doesn't use it yet.
 *
 *   void callback(int event, void *data) {
 *       ACH_UNUSED(data);
 *       handle_event(event);
 *   }
 * ------------------------------------------------------------------------- */
#define ACH_UNUSED(x) ((void)(x))

/* ---- Stringify ----------------------------------------------------------
 * Converts a macro argument to a string literal. Useful for embedding
 * version numbers, enum names, etc. into strings.
 *   #define VERSION 3
 *   ACH_STRINGIFY(VERSION) -> "3"
 * Two levels of indirection needed for macro expansion to happen first.
 * ------------------------------------------------------------------------- */
#define ACH_STRINGIFY_IMPL(x) #x
#define ACH_STRINGIFY(x) ACH_STRINGIFY_IMPL(x)

/* ---- Concatenation ------------------------------------------------------ */
#define ACH_CONCAT_IMPL(a, b) a##b
#define ACH_CONCAT(a, b) ACH_CONCAT_IMPL(a, b)

/* ---- Alignment ----------------------------------------------------------
 * Rounds 'n' up to the next multiple of 'align'.
 * 'align' MUST be a power of 2.
 *
 * How it works:
 *   ACH_ALIGN_UP(13, 8)
 *   = (13 + 8 - 1) & ~(8 - 1)
 *   = 20 & ~7
 *   = 20 & 0xFFFFFFF8
 *   = 16
 *
 * Used by the arena allocator to ensure allocations start at aligned
 * addresses (important for performance: misaligned access can be 2-10x
 * slower on some architectures, and on ARM it can trap).
 * ------------------------------------------------------------------------- */
#define ACH_ALIGN_UP(n, align) (((n) + (align) - 1) & ~((align) - 1))

/* ---- Kilobytes / Megabytes / Gigabytes ---------------------------------- */
#define ACH_KB(n) ((usize)(n) * 1024ULL)
#define ACH_MB(n) ((usize)(n) * 1024ULL * 1024ULL)
#define ACH_GB(n) ((usize)(n) * 1024ULL * 1024ULL * 1024ULL)

/* ---- Static Assert (C11+) -----------------------------------------------
 * Compile-time assertion. Fails the build if the condition is false.
 *   ACH_STATIC_ASSERT(sizeof(u32) == 4, "u32 must be 4 bytes");
 * ------------------------------------------------------------------------- */
#define ACH_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)

/* ---- Likely / Unlikely (Branch Hints) -----------------------------------
 * Hints to the compiler about branch prediction. On MSVC these are no-ops
 * (MSVC uses PGO instead of manual hints), but we define them for
 * documentation value — they communicate programmer intent.
 *
 *   if (ACH_UNLIKELY(ptr == NULL)) { handle_error(); }
 * ------------------------------------------------------------------------- */
#if defined(__GNUC__) || defined(__clang__)
    #define ACH_LIKELY(x)   __builtin_expect(!!(x), 1)
    #define ACH_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define ACH_LIKELY(x)   (x)
    #define ACH_UNLIKELY(x) (x)
#endif

#endif /* ACH_MACROS_H */
