#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([ripncode], [0.0.1], [kli@iki.fi])

AC_CONFIG_SRCDIR([src])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([src/config.h])

AM_INIT_AUTOMAKE([-Wno-portability subdir-objects foreign])
AC_SUBST(ACLOCAL_AMFLAGS, "-I m4")


# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AM_PROG_LIBTOOL

# Make first invocation of PKG_CHECK_MODULES 'if-then-else-fi'-safe.
PKG_PROG_PKG_CONFIG

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([string.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T
AC_CHECK_TYPES([ptrdiff_t])

# Check and enable extra compiler warnings if they are supported.
WARNING_CFLAGS=""
warncflags="-Wall -Wextra"
save_CPPFLAGS="$CPPFLAGS"
for opt in $warncflags; do
    AC_PREPROC_IFELSE([AC_LANG_PROGRAM([])],
                      [WARNING_CFLAGS="$WARNING_CFLAGS $opt"])
done
CPPFLAGS="$save_CPPFLAGS"

AC_SUBST(WARNING_CFLAGS)


# Checks for library functions.
AC_CHECK_FUNCS([strrchr strtoul])

# Check for murphy-common.
PKG_CHECK_MODULES(MURPHY, murphy-common, [have_murphy=yes], [have_murphy=no])

if test "$have_murphy" = "no"; then
    AC_MSG_ERROR([Murphy common headers/libraries not found.])
fi

AC_SUBST(MURPHY_CFLAGS)
AC_SUBST(MURPHY_LIBS)

# Check for libcdio.
PKG_CHECK_MODULES(CDIO, [libcdio_paranoia libcdio_cdda],
                        [have_cdio=yes], [have_cdio=no])

if test "$have_cdio" = "no"; then
    AC_MSG_ERROR([cdio-paranoia/cdda headers/libraries not found.])
fi

AC_SUBST(CDIO_CFLAGS)
AC_SUBST(CDIO_LIBS)

# Check for FLAC.
PKG_CHECK_MODULES(FLAC, flac, [have_flac=yes], [have_flac=no])

if test "$have_flac" = "no"; then
  AC_MSG_ERROR([FLAC headers/libraries not found.])
fi

AC_SUBST(FLAC_CFLAGS)
AC_SUBST(FLAC_LIBS)

# Check for libebur128
AC_CHECK_HEADERS(ebur128.h,
                 [have_ebur128=yes], [have_ebur128=no])

if test "$have_ebur128" = "no"; then
  AC_MSG_ERROR([libebur128 header file ebur128.h not found.])
fi

EBUR128_CFLAGS=""
EBUR128_LIBS="-lebur128 -lm"

AC_SUBST(EBUR128_CFLAGS)
AC_SUBST(EBUR128_LIBS)

# Check for the check test framework.
PKG_CHECK_MODULES(CHECK, check, [have_check=yes], [have_check=no])

AM_CONDITIONAL(HAVE_CHECK, test "$have_check" = "yes")
AC_SUBST(CHECK_CFLAGS)
AC_SUBST(CHECK_LIBS)

# Shave by default.
SHAVE_INIT([build-aux], [enable])

AC_CONFIG_FILES([build-aux/shave
                 build-aux/shave-libtool
                 Makefile
                 src/Makefile
                ])

AC_OUTPUT
