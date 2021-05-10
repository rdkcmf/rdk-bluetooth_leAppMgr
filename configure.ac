##########################################################################
# If not stated otherwise in this file or this component's Licenses.txt
# file the following copyright and licenses apply:
#
# Copyright 2018 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
##########################################################################
#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([btr-leApp-mgr], [0.1.0], [rajnish_dey2@cable.comcast.com])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([cfg])
AM_INIT_AUTOMAKE([subdir-objects -Wall -Werror foreign no-dist-gzip dist-bzip2 1.9])

m4_ifdef([AM_PROG_AR], [AM_PROG_AR])

LT_INIT
AC_LANG([C])

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])],
[AM_DEFAULT_VERBOSITY=1
AC_SUBST(AM_DEFAULT_VERBOSITY)])


# Checks for programs.
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PROG_CC


AC_CONFIG_SRCDIR([src/btrLeMgr.cpp])
AC_CONFIG_HEADERS([config.h])

PKG_CHECK_MODULES([LIBCJSON], [libcjson >= 1.0])

RDK_LOGGER_FLAG=" "
OAUTH_SUPPORT=" "

# Check for breakpad
BREAKPAD_CFLAGS=" "
BREAKPAD_LFLAGS=" "
AC_ARG_ENABLE([breakpad],
        AS_HELP_STRING([--enable-breakpad],[enable breakpad support (default is no)]),
        [
            case "${enableval}" in
             yes) BREAKPAD_CFLAGS="-DINCLUDE_BREAKPAD"
                    BREAKPAD_LFLAGS="-lbreakpadwrapper";;
             no) AC_MSG_ERROR([breakpad is disabled]) ;;
             *) AC_MSG_ERROR([bad value ${enableval} for --enable-breakpad]) ;;
             esac
             ],
            [echo "breakpad is disabled"])

AC_SUBST(BREAKPAD_CFLAGS)
AC_SUBST(BREAKPAD_LFLAGS)

# Check for rdk logger
 AC_ARG_ENABLE([rdk-logger],
               AS_HELP_STRING([--enable-rdk-logger],[enable rdk logger (default is no)]),
               [
                 case "${enableval}" in
                  yes) rdk_logger_ok=yes
                    RDK_LOGGER_FLAG="-DRDK_LOGGER_ENABLED" ;;
                  no)  rdk_logger_ok=no;;
                  *) AC_MSG_ERROR([bad value ${enableval} for --enable-rdk-logger]) ;;
                 esac
               ],
               [echo "rdk logger is disabled"])
AS_IF([test "x$rdk_logger_ok" = "xyes"], [
        AC_CHECK_LIB(rdkloggers, rdk_logger_init,
                       enable_rdk_logger=yes, enable_rdk_logger=no
                       AC_MSG_ERROR([RDK_LOGGER library (librdkloggers) not found]))
if test "x$enable_rdk_logger" = "xyes"; then
    AC_MSG_CHECKING([for rdkloggers])
       AC_TRY_COMPILE(
    [#include <stdio.h>
    #undef PACKAGE
    #undef VERSION
    #undef HAVE_STDLIB_H
    #include "rdk_debug.h"],,
             enable_rdk_logger=yes,
             enable_rdk_logger=no)
       AC_MSG_RESULT($enable_rdk_logger)
       if test "x$enable_rdk_logger" = "xyes"; then
         AC_DEFINE(RDK_LOGGER_ENABLED, 1, [Enabled RDK Logger]) 
       fi
fi
], [])

AM_CONDITIONAL([WITH_RDK_LOGGER], [test x$rdk_logger_ok = xyes])
AC_SUBST(RDK_LOGGER_FLAG)

AC_ARG_ENABLE([oauthsupport],
        AS_HELP_STRING([--enable-oauthsupport],[This will enable the oauth client supported. ]),
        [
          case "${enableval}" in
           yes) OAUTH_SUPPORT=true ;;
           no) OAUTH_SUPPORT=false ;;
          *) AC_MSG_ERROR([bad value ${enableval} for --enable-oauthsupport ]) ;;
           esac
           ],
         [echo " SMART MON Tool is enable"])
AM_CONDITIONAL([BTR_LOCAL_OAUTH_SUPPORT], [test x$OAUTH_SUPPORT = xtrue])

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h unistd.h])

# Use JSON
#PKG_CHECK_MODULES([LIBCJSON], [libcjson >= 1.0])
PKG_CHECK_MODULES([CURL],[libcurl >= 7.19.6])

BTR_LE_MGR_SRC="src"
#BTR_LE_MGR_TEST="tests"


# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_SUBST(BTR_LE_MGR_SRC)
#AC_SUBST(BTR_LE_MGR_TEST)


AC_SUBST(BTR_LE_MGR_SRC)

# Checks for library functions.
AC_CONFIG_FILES([Makefile
                 src/Makefile
                 tests/Makefile])


AC_OUTPUT
