AC_INIT([shr-extras], 1.0)
AC_CONFIG_MACRO_DIRS([m4])
dnl need AC_GNU_SOURCE for SCM_CREDENTIALS and recvmmsg
AC_GNU_SOURCE
AM_INIT_AUTOMAKE([foreign subdir-objects])
m4_ifdef([AM_SILENT_RULES],
    [AM_SILENT_RULES([yes])
    ])
LT_INIT([shared])
AC_PROG_CC

have_shr_header=n
have_shr_lib=n
AC_CHECK_HEADERS([shr.h],[have_shr_header=y])
AC_CHECK_LIB(shr,shr_readv,[have_shr_lib=y])
if test "x${have_shr_header}${have_shr_lib}" != xyy
then
  AC_MSG_ERROR([
  -----------------------------------------------------
  The libshr build prerequisite was not found. Please
  see the build instructions, install libshr and retry.
  -----------------------------------------------------
  ])
fi


# is nanomsg installed
AC_CHECK_LIB(nanomsg,nn_socket,
  AM_CONDITIONAL(HAVE_NANOMSG,true),
  AM_CONDITIONAL(HAVE_NANOMSG,false))

# is ncurses installed
AC_CHECK_LIB(ncurses,initscr,
  AM_CONDITIONAL(HAVE_NCURSES,true),
  AM_CONDITIONAL(HAVE_NCURSES,false))

AC_CONFIG_FILES([Makefile
   util/Makefile
   ])
AC_OUTPUT
