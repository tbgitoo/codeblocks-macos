AM_CPPFLAGS = $(WX_CXXFLAGS) \
        -I$(top_srcdir)/src/include \
        -I$(top_srcdir)/src/src \
        -I$(top_srcdir)/src/sdk/wxscintilla/include

pluginlibdir = $(pkglibdir)/plugins

pluginlib_LTLIBRARIES = libdragscroll.la

libdragscroll_la_LDFLAGS = @MODULE_SHARED_LDFLAGS@ -version-info 0:1:0 -no-undefined -avoid-version

libdragscroll_la_LIBADD = ../../../sdk/libcodeblocks.la $(WX_LIBS)

libdragscroll_la_SOURCES = src/dragscroll.cpp \
            src/dragscrollcfg.cpp \
            src/dragscrollevent.cpp \
            src/linux_functions.cpp

noinst_HEADERS = src/dragscroll.h \
            src/dragscrollcfg.h \
            src/dragscrollevent.h \
            src/linux_functions.h

EXTRA_DIST = $(srcdir)/*.cbp \
            src/manifest.xml

pkgdata_DATA = dragscroll.zip
CLEANFILES = $(pkgdata_DATA)

dragscroll.zip: $(srcdir)/src/manifest.xml
	$(am__cd) $(srcdir) && zip $(abs_builddir)/dragscroll.zip src/manifest.xml > /dev/null
