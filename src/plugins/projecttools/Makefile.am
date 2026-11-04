AM_CPPFLAGS = $(WX_CXXFLAGS) \
			-I$(top_srcdir)/src/include \
			-I$(top_srcdir)/src/sdk/wxscintilla/include

pluginlibdir = $(pkglibdir)/plugins

pluginlib_LTLIBRARIES = libprojecttools.la

libprojecttools_la_LDFLAGS = @MODULE_SHARED_LDFLAGS@ -version-info 0:1:0 -no-undefined -avoid-version

libprojecttools_la_LIBADD = ../../sdk/libcodeblocks.la $(WX_LIBS)

libprojecttools_la_SOURCES = \
	projecttools.cpp \
	projecttoolcontext.cpp \
	toolkitdetection.cpp

noinst_HEADERS = \
	projecttools.h \
	projecttoolcontext.h \
	toolkitprovider.h \
	toolkitdetection.h
	
	
EXTRA_DIST = manifest.xml

pkgdata_DATA = projecttools.zip

CLEANFILES = $(pkgdata_DATA)

projecttools.zip: $(EXTRA_DIST)
	PWD=`pwd` cd $(srcdir) && zip $(PWD)/projecttools.zip manifest.xml > /dev/null