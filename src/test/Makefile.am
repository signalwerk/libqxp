## -*- Mode: make; tab-width: 4; indent-tabs-mode: tabs -*-

target_test = test

check_PROGRAMS = $(target_test)

AM_CXXFLAGS = \
	-DDETECTION_TEST_DIR=\"$(top_srcdir)/src/test/data\" \
	-I$(top_srcdir)/inc \
	-I$(top_srcdir)/src/lib \
	$(CPPUNIT_CFLAGS) \
	$(REVENGE_CFLAGS) \
	$(REVENGE_STREAM_CFLAGS) \
	$(DEBUG_CXXFLAGS)

test_LDFLAGS = -L$(top_srcdir)/src/lib
test_LDADD = \
	$(top_builddir)/src/lib/libqxp-@QXP_MAJOR_VERSION@.@QXP_MINOR_VERSION@.la \
	$(CPPUNIT_LIBS) \
	$(REVENGE_LIBS) \
	$(REVENGE_STREAM_LIBS)

test_SOURCES = \
	test.cpp \
	QXPDocumentTest.cpp


TESTS = $(target_test)

EXTRA_DIST = \
	data/qxp33.qxd \
	data/qxp4.qxd \
	data/qxp5.qxd \
	data/qxp6.qxd \
	data/unsupported.zip

## vim:set shiftwidth=4 tabstop=4 noexpandtab:
