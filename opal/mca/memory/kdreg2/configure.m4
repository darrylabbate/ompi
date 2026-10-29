# -*- shell-script -*-
#
# Copyright (c) 2026      Amazon.com, Inc. or its affiliates.
#                         All Rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
# SPDX-License-Identifier: BSD-3-Clause-Open-MPI
#

AC_DEFUN([MCA_opal_memory_kdreg2_PRIORITY], [50])

AC_DEFUN([MCA_opal_memory_kdreg2_COMPILE_MODE], [
    AC_MSG_CHECKING([for MCA component $2:$3 compile mode])
    $4="static"
    AC_MSG_RESULT([$$4])
])

# MCA_opal_memory_kdreg2_CONFIG(action-if-can-compile,
#                               [action-if-cant-compile])
# ------------------------------------------------
AC_DEFUN([MCA_opal_memory_kdreg2_CONFIG],[
    AC_ARG_WITH([kdreg2],
        [AS_HELP_STRING([--with-kdreg2(=DIR)],
                        [Build the kdreg2 memory hooks component, optionally adding DIR to the include path to find kdreg2.h])])

    opal_memory_kdreg2_happy=no
    opal_memory_kdreg2_CPPFLAGS=

    AS_IF([test -n "$with_memory_manager" && test "$with_memory_manager" != "yes" && test "$with_memory_manager" != "kdreg2"],
          [with_kdreg2=no])

    AS_IF([test "$with_kdreg2" != "no"], [
        opal_memory_kdreg2_save_CPPFLAGS=$CPPFLAGS
        AS_IF([test -n "$with_kdreg2" && test "$with_kdreg2" != "yes"],
              [opal_memory_kdreg2_CPPFLAGS="-I$with_kdreg2"
               CPPFLAGS="$CPPFLAGS $opal_memory_kdreg2_CPPFLAGS"])

        AC_CHECK_HEADERS([kdreg2.h linux/kdreg2.h], [opal_memory_kdreg2_happy=yes; break])

        CPPFLAGS=$opal_memory_kdreg2_save_CPPFLAGS
    ])

    AS_IF([test "$opal_memory_kdreg2_happy" = "no" && test -n "$with_kdreg2" && test "$with_kdreg2" != "no"],
          [AC_MSG_ERROR([kdreg2 support requested but kdreg2.h was not found])])
    AS_IF([test "$opal_memory_kdreg2_happy" = "no" && test "$with_memory_manager" = "kdreg2"],
          [AC_MSG_ERROR([--with-memory-manager=kdreg2 requested but kdreg2.h was not found])])

    AC_SUBST([memory_kdreg2_CPPFLAGS], [$opal_memory_kdreg2_CPPFLAGS])

    AS_IF([test "$opal_memory_kdreg2_happy" = "yes"], [$1], [$2])

    AC_CONFIG_FILES([opal/mca/memory/kdreg2/Makefile])
])
