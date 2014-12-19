#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.58)
AC_INIT(env, 0.0.0, opensource@qeo.org)
AC_CANONICAL_SYSTEM 
AC_CONFIG_SRCDIR([supported_targets/configure.env.in])

# Checks for programs.
AC_PROG_CC
AC_PROG_CPP
AC_PROG_CXX
AC_PROG_LN_S
AC_PROG_LIBTOOL

AC_PATH_PROG( SED, sed, , $PATH:/usr/bin:/usr/local/bin)
if test -z "$SED"; then
  AC_MSG_ERROR([sed could not be found, please install])
fi
	
AM_PROG_CC_C_O
AC_C_BIGENDIAN(ENDIAN=BIG,ENDIAN=LITTLE)

CC=`which $CC`
TOOLCHAIN_PREFIX=`echo $CC | $SED "s/gcc$//g"`
HOSTID=`uname -m`_`uname -s`
test "$ac_cv_prog_cc_g" = yes && EXTRADEBUG="-g"

CFLAGS=`echo $CFLAGS | $SED "s/ $EXTRADEBUG //g" | $SED "s/ $EXTRADEBUG$//g" | $SED "s/^$EXTRADEBUG //g"`

TARGET_CPU=`echo $target_cpu | tr "[a-z]" "[A-Z]"`
TARGET_OS=`echo $target_os | $SED "s/-gnu$//g" | tr "[a-z]" "[A-Z]"`

case "$pic_mode" in
	yes|default) E_SHARED_LIB_CFLAGS=$lt_cv_prog_cc_pic$lt_prog_compiler_pic;E_SHARED_DIR_INFIX=/shared;;
	no)
	           case "${host}" in
	           	x86_64-*-*|ia64-*-*) E_SHARED_LIB_CFLAGS=$lt_cv_prog_cc_pic$lt_prog_compiler_pic;E_SHARED_DIR_INFIX=/shared;;
	           esac;;
	*) AC_MSG_ERROR([Invalid setting for with-pic. Use "yes", "no" or "default"]) ;;
esac

AC_SUBST(E_SHARED_LIB_CFLAGS)
AC_SUBST(E_SHARED_DIR_INFIX)

rt_LIBS=

AC_CHECK_LIB(rt, clock_gettime, [rt_LIBS=-lrt])

AC_SUBST(rt_LIBS)
				
PKG_CONFIG_SYSROOT_DIR=`echo $PKG_CONFIG_SYSROOT_DIR`
AC_SUBST(PKG_CONFIG_SYSROOT_DIR)

PKG_CONFIG_PATH=`echo $PKG_CONFIG_PATH`
AC_SUBST(PKG_CONFIG_PATH)

PATH=`echo $PATH`
AC_SUBST(PATH)


# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_SIZEOF(long)

AC_SUBST(target)
AC_SUBST(ENDIAN)
AC_SUBST(EXTRADEBUG)
AC_SUBST(HOSTID)
AC_SUBST(TARGET_CPU)
AC_SUBST(TARGET_OS)
AC_SUBST(TOOLCHAIN_PREFIX)
AC_SUBST(ac_cv_sizeof_long)

# Checks for library functions.

AC_CONFIG_FILES([$srcdir/supported_targets/configure.env])

test -e $srcdir/supported_hosts/$HOSTID.host || echo 'OUTPUT_PREFIX    := $(projectdir/)output' > $srcdir/supported_hosts/$HOSTID.host
                
AC_OUTPUT
