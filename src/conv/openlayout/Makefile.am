## -*- Mode: make; tab-width: 4; indent-tabs-mode: tabs -*-

bin_PROGRAMS = qxp2openlayout

AM_CXXFLAGS = \
	-I$(top_srcdir)/inc \
	-I$(top_srcdir)/src/lib \
	$(REVENGE_CFLAGS) \
	$(REVENGE_STREAM_CFLAGS) \
	$(BOOST_CFLAGS) \
	$(DEBUG_CXXFLAGS)

qxp2openlayout_DEPENDENCIES = @QXP2OPENLAYOUT_WIN32_RESOURCE@
# the converter needs the document structure (masters, stylesheets, text
# chains), which is not exposed by the public API, so it uses the internal library
qxp2openlayout_LDADD = \
	$(top_builddir)/src/lib/libqxp_internal.la \
	$(REVENGE_LIBS) \
	$(REVENGE_STREAM_LIBS) \
	$(ICU_LIBS) \
	@QXP2OPENLAYOUT_WIN32_RESOURCE@

qxp2openlayout_SOURCES = \
	JsonValue.cpp \
	JsonValue.h \
	OpenLayoutCollector.cpp \
	OpenLayoutCollector.h \
	qxp2openlayout.cpp

if OS_WIN32

@QXP2OPENLAYOUT_WIN32_RESOURCE@ : qxp2openlayout.rc $(qxp2openlayout_OBJECTS)
	chmod +x $(top_srcdir)/build/win32/*compile-resource && \
	WINDRES=@WINDRES@ $(top_srcdir)/build/win32/lt-compile-resource qxp2openlayout.rc @QXP2OPENLAYOUT_WIN32_RESOURCE@

endif

EXTRA_DIST = \
	$(qxp2openlayout_SOURCES) \
	qxp2openlayout.rc.in

# These may be in the builddir too
BUILD_EXTRA_DIST = \
	qxp2openlayout.rc

## vim:set shiftwidth=4 tabstop=4 noexpandtab:
