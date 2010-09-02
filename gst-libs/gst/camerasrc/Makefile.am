
lib_LTLIBRARIES = libgstcamerasrc-@GST_MAJORMINOR@.la

CLEANFILES = $(BUILT_SOURCES)

libgstcamerasrc_@GST_MAJORMINOR@_la_SOURCES = \
	gstcamerabin-enum.c \
	gstbasecamerasrc.c

libgstcamerasrc_@GST_MAJORMINOR@includedir = $(includedir)/gstreamer-@GST_MAJORMINOR@/gst/camerasrc
libgstcamerasrc_@GST_MAJORMINOR@include_HEADERS = \
	gstcamerabin-enum.h \
	gstbasecamerasrc.h

libgstcamerasrc_@GST_MAJORMINOR@_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_CFLAGS) -DGST_USE_UNSTABLE_API -I$(top_srcdir)/gst-libs -I$(top_builddir)/gst-libs
libgstcamerasrc_@GST_MAJORMINOR@_la_LIBADD = \
	$(top_builddir)/gst-libs/gst/interfaces/libgstphotography-$(GST_MAJORMINOR).la \
	$(GST_LIBS) $(GST_BASE_LIBS) $(GST_PLUGINS_BASE_LIBS) \
	-lgstinterfaces-$(GST_MAJORMINOR)

libgstcamerasrc_@GST_MAJORMINOR@_la_LDFLAGS = $(GST_LIB_LDFLAGS) $(GST_ALL_LDFLAGS) $(GST_LT_LDFLAGS)

