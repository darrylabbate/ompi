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

AM_CPPFLAGS = $(memory_kdreg2_CPPFLAGS)

noinst_LTLIBRARIES = libmca_memory_kdreg2.la
libmca_memory_kdreg2_la_SOURCES = \
    memory_kdreg2.h \
    memory_kdreg2_component.c
libmca_memory_kdreg2_la_LDFLAGS = \
   -module -avoid-version $(memory_kdreg2_LDFLAGS)
libmca_memory_kdreg2_la_LIBADD = $(memory_kdreg2_LIBS)
