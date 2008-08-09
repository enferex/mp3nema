#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT

# Checks for programs.
AC_PROG_CC
AC_LANG_C

# Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_CHECK_HEADERS([netdb.h stdlib.h string.h sys/socket.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_STAT
AC_CHECK_FUNCS([gethostbyname memmove memset mkdir \
                socket strchr strdup strstr strrchr])

# Macros
CFLAGS="-Wall"
AC_ARG_ENABLE([debug],
              [AC_HELP_STRING([--enable-debug], [Enable debugging output])],
              [CFLAGS="$CFLAGS -g3 -DDEBUG"],
              [CFLAGS="$CFLAGS -O3"])

AC_OUTPUT([Makefile])
