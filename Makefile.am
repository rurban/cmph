SUBDIRS = src tests examples man docs
EXTRA_DIST = cmph.spec configure.ac cmph.pc.in cxxmph.pc.in LGPL-2 MPL-1.1 README
pkgconfig_DATA = cmph.pc
if USE_CXXMPH
  SUBDIRS += cxxmph
  pkgconfig_DATA += cxxmph.pc
endif
ACLOCAL_AMFLAGS="-Im4"

pkgconfigdir = $(libdir)/pkgconfig
#README: docs/README.t2t
#	cd docs && $(MAKE) -C docs index.html

CLEANFILES = TAGS
DISTCLEANFILES = README
