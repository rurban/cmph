SUBDIRS = src tests examples man
EXTRA_DIST = cmph.spec configure.ac cmph.pc.in cxxmph.pc.in LGPL-2 MPL-1.1 docs/*
pkgconfig_DATA = cmph.pc
if USE_CXXMPH
  SUBDIRS += cxxmph
  pkgconfig_DATA += cxxmph.pc
endif
ACLOCAL_AMFLAGS="-Im4"

pkgconfigdir = $(libdir)/pkgconfig

CLEANFILES = TAGS
