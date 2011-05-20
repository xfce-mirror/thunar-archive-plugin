# $Id$

INCLUDES =								\
	-I$(top_builddir)						\
	-I$(top_srcdir)							\
	-DG_LOG_DOMAIN=\"thunar-archive-plugin\"			\
	-DEXO_API_SUBJECT_TO_CHANGE					\
	-DLIBEXECDIR=\"$(libexecdir)\"					\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"				\
	$(PLATFORM_CPPFLAGS)

extensionsdir = $(libdir)/thunarx-2
extensions_LTLIBRARIES =						\
	thunar-archive-plugin.la

thunar_archive_plugin_la_SOURCES =					\
	tap-backend.c							\
	tap-backend.h							\
	tap-provider.c							\
	tap-provider.h							\
	thunar-archive-plugin.c

thunar_archive_plugin_la_CFLAGS =					\
	$(PLATFORM_CFLAGS)						\
	$(THUNARX_CFLAGS)						\
	$(EXO_CFLAGS)							\
	$(LIBXFCE4UTIL_CFLAGS)

thunar_archive_plugin_la_LIBADD =					\
	$(PLATFORM_LIBS)						\
	$(THUNARX_LIBS)							\
	$(EXO_LIBS)							\
	$(LIBXFCE4UTIL_LIBS)

thunar_archive_plugin_la_LDFLAGS =					\
	-avoid-version							\
	-export-dynamic							\
	-module								\
	$(PLATFORM_LDFLAGS)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
