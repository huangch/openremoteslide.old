EXTRA_DIST = README.txt lgpl-2.1.txt LICENSE.txt CHANGELOG.txt doc/Doxyfile CONTRIBUTING.txt

# doxygen
dist-hook:
	cd $(distdir)/doc; doxygen

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = openremoteslide.pc

ACLOCAL_AMFLAGS=-I m4


lib_LTLIBRARIES = src/libopenremoteslide.la

src_libopenremoteslide_la_LIBADD = $(GLIB2_LIBS) $(CAIRO_LIBS) $(SQLITE3_LIBS) \
	$(LIBXML2_LIBS) $(OPENJPEG_LIBS) $(LIBTIFF_LIBS) $(LIBPNG_LIBS) \
	$(GDKPIXBUF_LIBS) $(ZLIB_LIBS)

src_libopenremoteslide_la_SOURCES = \
	src/openremoteslide.c \
	src/openremoteslide-cache.c \
	src/openremoteslide-decode-gdkpixbuf.c \
	src/openremoteslide-decode-jp2k.c \
	src/openremoteslide-decode-jpeg.c \
	src/openremoteslide-decode-png.c \
	src/openremoteslide-decode-sqlite.c \
	src/openremoteslide-decode-tiff.c \
	src/openremoteslide-decode-tifflike.c \
	src/openremoteslide-decode-xml.c \
	src/openremoteslide-error.c \
	src/openremoteslide-grid.c \
	src/openremoteslide-hash.c \
	src/openremoteslide-jdatasrc.c \
	src/openremoteslide-tables.c \
	src/openremoteslide-util.c \
	src/openremoteslide-vendor-aperio.c \
	src/openremoteslide-vendor-generic-tiff.c \
	src/openremoteslide-vendor-hamamatsu.c \
	src/openremoteslide-vendor-leica.c \
	src/openremoteslide-vendor-mirax.c \
	src/openremoteslide-vendor-philips.c \
	src/openremoteslide-vendor-sakura.c \
	src/openremoteslide-vendor-trestle.c \
	src/openremoteslide-vendor-ventana.c

EXTRA_PROGRAMS = src/make-tables
CLEANFILES = src/make-tables
MAINTAINERCLEANFILES = src/openremoteslide-tables.c
# Depending directly on src/make-tables causes needless regeneration of
# openremoteslide-tables.c.  As the lesser of evils, recursively invoke make.
src/openremoteslide-tables.c: src/make-tables.c
	@$(MAKE) $(AM_MAKEFLAGS) src/make-tables
	$(AM_V_GEN)src/make-tables$(EXEEXT) "$@"

if WINDOWS_RESOURCES
src_libopenremoteslide_la_SOURCES += src/openremoteslide-dll.rc
src/openremoteslide-dll.lo: src/openremoteslide-dll.manifest
endif

.rc.lo:
	$(AM_V_GEN)$(LIBTOOL) $(AM_V_lt) $(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) --tag=RC --mode=compile $(RC) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) -i "$<" -o "$@"

src_libopenremoteslide_la_CPPFLAGS = -pedantic -D_OPENSLIDE_BUILDING_DLL \
	$(GLIB2_CFLAGS) $(CAIRO_CFLAGS) $(SQLITE3_CFLAGS) $(LIBXML2_CFLAGS) \
	$(OPENJPEG_CFLAGS) $(LIBTIFF_CFLAGS) $(LIBPNG_CFLAGS) \
	$(GDKPIXBUF_CFLAGS) $(ZLIB_CFLAGS) -DG_LOG_DOMAIN=\"Openslide\" \
	-I$(top_srcdir)/src

src_libopenremoteslide_la_LDFLAGS = -version-info 4:1:4 -no-undefined


pkginclude_HEADERS = \
	src/openremoteslide.h \
	src/openremoteslide-features.h

noinst_HEADERS = \
	src/openremoteslide-cairo.h \
	src/openremoteslide-decode-gdkpixbuf.h \
	src/openremoteslide-decode-jp2k.h \
	src/openremoteslide-decode-jpeg.h \
	src/openremoteslide-decode-png.h \
	src/openremoteslide-decode-sqlite.h \
	src/openremoteslide-decode-tiff.h \
	src/openremoteslide-decode-tifflike.h \
	src/openremoteslide-decode-xml.h \
	src/openremoteslide-error.h \
	src/openremoteslide-hash.h \
	src/openremoteslide-private.h \
	test/test-common.h \
	tools/openremoteslide-tools-common.h



noinst_PROGRAMS = test/test test/try_open test/parallel test/query \
	test/extended test/mosaic test/profile
noinst_SCRIPTS = test/driver
CLEANFILES += test/driver
EXTRA_DIST += test/driver.in

test_test_CPPFLAGS = $(GLIB2_CFLAGS) $(CAIRO_CFLAGS) $(VALGRIND_CFLAGS) -I$(top_srcdir)/src
# VALGRIND_LIBS not needed
test_test_LDADD = src/libopenremoteslide.la $(GLIB2_LIBS) $(CAIRO_LIBS)

test_try_open_SOURCES = test/test-common.c test/try_open.c
test_try_open_CPPFLAGS = $(GLIB2_CFLAGS) -I$(top_srcdir)/src
test_try_open_LDADD = src/libopenremoteslide.la $(GLIB2_LIBS)

test_parallel_CPPFLAGS = $(GLIB2_CFLAGS) -I$(top_srcdir)/src
test_parallel_LDADD = src/libopenremoteslide.la $(GLIB2_LIBS)

test_query_CPPFLAGS = $(GLIB2_CFLAGS) -I$(top_srcdir)/src
test_query_LDADD = src/libopenremoteslide.la $(GLIB2_LIBS)

test_extended_SOURCES = test/test-common.c test/extended.c
test_extended_CPPFLAGS = $(GLIB2_CFLAGS) -I$(top_srcdir)/src
test_extended_LDADD = src/libopenremoteslide.la $(GLIB2_LIBS)

test_mosaic_CPPFLAGS = $(GLIB2_CFLAGS) $(CAIRO_CFLAGS) -I$(top_srcdir)/src
test_mosaic_LDADD = src/libopenremoteslide.la $(GLIB2_LIBS) $(CAIRO_LIBS)

test_profile_CPPFLAGS = $(GLIB2_CFLAGS) $(VALGRIND_CFLAGS) -I$(top_srcdir)/src
test_profile_LDADD = src/libopenremoteslide.la $(GLIB2_LIBS)

if CYGWIN_CROSS_TEST
noinst_PROGRAMS += test/symlink
test_symlink_LDADD = -lkernel32
endif

test/driver: test/driver.in Makefile
	$(AM_V_GEN)sed -e 's:!!SRCDIR!!:$(abs_srcdir)/test:g' \
		-e 's:!!BUILDDIR!!:$(abs_builddir)/test:g' \
		-e 's:!!VERSION!!:$(VERSION):g' \
		-e 's:!!CYGWIN_CROSS_TEST!!:$(CYGWIN_CROSS_TEST):g' \
		-e 's:!!FEATURES!!:$(FEATURE_FLAGS):g' "$<" > "$@" && \
		chmod +x "$@"


# tools

# show-properties
bin_PROGRAMS = tools/openremoteslide-show-properties
man_MANS = tools/openremoteslide-show-properties.1
tools_openremoteslide_show_properties_SOURCES = tools/openremoteslide-tools-common.c tools/openremoteslide-show-properties.c
tools_openremoteslide_show_properties_CPPFLAGS = -I$(top_srcdir)/src $(GLIB2_CFLAGS)
tools_openremoteslide_show_properties_LDADD = src/libopenremoteslide.la $(GLIB2_LIBS)

# quickhash1sum
bin_PROGRAMS += tools/openremoteslide-quickhash1sum
man_MANS += tools/openremoteslide-quickhash1sum.1
tools_openremoteslide_quickhash1sum_SOURCES = tools/openremoteslide-tools-common.c tools/openremoteslide-quickhash1sum.c
tools_openremoteslide_quickhash1sum_CPPFLAGS = -I$(top_srcdir)/src $(GLIB2_CFLAGS)
tools_openremoteslide_quickhash1sum_LDADD = src/libopenremoteslide.la $(GLIB2_LIBS)

# write-png
bin_PROGRAMS += tools/openremoteslide-write-png
man_MANS += tools/openremoteslide-write-png.1
tools_openremoteslide_write_png_SOURCES = tools/openremoteslide-tools-common.c tools/openremoteslide-write-png.c
tools_openremoteslide_write_png_CPPFLAGS = -I$(top_srcdir)/src $(LIBPNG_CFLAGS) $(GLIB2_CFLAGS)
tools_openremoteslide_write_png_LDADD = src/libopenremoteslide.la $(LIBPNG_LIBS) $(GLIB2_LIBS)

# man pages
EXTRA_DIST += $(man_MANS:=.in)
