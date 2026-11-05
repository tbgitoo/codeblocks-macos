AM_CPPFLAGS = $(WX_CXXFLAGS) \
			-I$(top_srcdir)/src/include \
			-I$(top_srcdir)/src/sdk/wxscintilla/include

pluginlibdir = $(pkglibdir)/plugins

pluginlib_LTLIBRARIES = libogretools.la

libogretools_la_LDFLAGS = @MODULE_SHARED_LDFLAGS@ -version-info 0:1:0 -no-undefined -avoid-version

libogretools_la_LIBADD = ../../sdk/libcodeblocks.la $(WX_LIBS)

libogretools_la_SOURCES = \
	ogretools.cpp \
	ogretoolslocator.cpp

noinst_HEADERS = \
	ogretools.h \
	ogretoolslocator.h

EXTRA_DIST = manifest.xml

pkgdata_DATA = ogretools.zip

CLEANFILES = $(pkgdata_DATA)

ogretools.zip: $(EXTRA_DIST)
	PWD=`pwd` cd $(srcdir) && zip $(PWD)/ogretools.zip manifest.xml > /dev/null
