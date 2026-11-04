#pragma once

// latch is written for GCC 16's C++26 mode. Older GCC is missing pieces of
// that draft, and other compilers are not the ones this tree is built with.
#if !defined(__GNUC__) || defined(__clang__)
#error "latch requires GCC"
#endif
#if __GNUC__ < 16
#error "latch requires GCC 16 or newer"
#endif
