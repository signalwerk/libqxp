## -*- Mode: make; tab-width: 4; indent-tabs-mode: tabs -*-

if PLATFORM_WIN32
version_info = -version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) -avoid-version
else
version_info = -version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)
endif

lib_LTLIBRARIES = libqxp-@QXP_MAJOR_VERSION@.@QXP_MINOR_VERSION@.la

AM_CXXFLAGS = \
	-DLIBQXP_BUILD=1 \
	-I$(top_srcdir)/inc \
	$(REVENGE_CFLAGS) \
	$(REVENGE_STREAM_CFLAGS) \
	$(BOOST_CFLAGS) \
	$(DEBUG_CXXFLAGS)

libqxp_@QXP_MAJOR_VERSION@_@QXP_MINOR_VERSION@_la_LIBADD = \
	$(REVENGE_LIBS) \
	@LIBQXP_WIN32_RESOURCE@

libqxp_@QXP_MAJOR_VERSION@_@QXP_MINOR_VERSION@_la_DEPENDENCIES = @LIBQXP_WIN32_RESOURCE@
libqxp_@QXP_MAJOR_VERSION@_@QXP_MINOR_VERSION@_la_LDFLAGS = $(version_info) -export-dynamic -no-undefined
libqxp_@QXP_MAJOR_VERSION@_@QXP_MINOR_VERSION@_la_SOURCES = \
	QXPDocument.cpp \
	QXPCollector.cpp \
	QXPCollector.h \
	QXPHeader.cpp \
	QXPHeader.h \
	QXP33Header.cpp \
	QXP33Header.h \
	QXP4Header.cpp \
	QXP4Header.h \
	QXPParser.cpp \
	QXPParser.h \
	QXP33Parser.cpp \
	QXP33Parser.h \
	QXP4Parser.cpp \
	QXP4Parser.h \
	QXPTypes.cpp \
	QXPTypes.h \
	libqxp_utils.cpp \
	libqxp_utils.h

if OS_WIN32

@LIBQXP_WIN32_RESOURCE@ : libqxp.rc $(libqxp_@QXP_MAJOR_VERSION@_@QXP_MINOR_VERSION@_la_OBJECTS)
	chmod +x $(top_srcdir)/build/win32/*compile-resource
	WINDRES=@WINDRES@ $(top_srcdir)/build/win32/lt-compile-resource libqxp.rc @LIBQXP_WIN32_RESOURCE@

endif

EXTRA_DIST = \
	libqxp.rc.in

# These may be in the builddir too
BUILD_EXTRA_DIST = \
	libqxp.rc

## vim:set shiftwidth=4 tabstop=4 noexpandtab:
