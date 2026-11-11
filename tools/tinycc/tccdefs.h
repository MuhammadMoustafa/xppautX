/*  tccdefs.h of xppautX (W700): the predefined macros and types of the one
    program TinyCC compiles, expr_native.cpp's generated C. It has no
    includes, no libc and no varargs: only doubles, ints, pointers to
    them, loops and calls, so TinyCC's own predefs (the glibc, BSD and
    Windows compatibility macros, the 128-bit and va_list types, about 300
    lines it preprocesses and parses on every model load) are not read.
    Same format as third_party/tinycc/include/tccdefs.h: conftest.c's C2STR
    turns the lines indented by 4 or more into the C strings tccpp.c
    includes, and replaces the platform macros of the lines starting at
    column 1 by the compile-time ones.  Nothing is defined before this file
    but the target machine and os and tcc_predefs()'s few macros. */

#if __SIZEOF_LONG__ == 4
    /* 64bit Windows. */
    #define __SIZE_TYPE__ unsigned long long
    #define __PTRDIFF_TYPE__ long long
#else
    #define __SIZE_TYPE__ unsigned long
    #define __PTRDIFF_TYPE__ long
#endif
