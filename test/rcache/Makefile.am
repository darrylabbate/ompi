#
# Copyright (c) 2026      Amazon.com, Inc. or its affiliates.  All Rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
# SPDX-License-Identifier: BSD-3-Clause-Open-MPI
#

TESTS = rcache_grdma_reuse

check_PROGRAMS = $(TESTS)

rcache_grdma_reuse_SOURCES = rcache_grdma_reuse.c

LDFLAGS = $(OPAL_PKG_CONFIG_LDFLAGS)
LDADD = $(top_builddir)/opal/lib@OPAL_LIB_NAME@.la

distclean-local:
	rm -rf *.dSYM .deps .libs *.log *.o *.trs $(check_PROGRAMS) Makefile
