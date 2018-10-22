# $Id$

wrapperdir = $(libexecdir)/thunar-archive-plugin
wrapper_SCRIPTS= 						\
	ark.tap								\
	engrampa.tap						\
	file-roller.tap

# Install symlink to 'gnome-file-roller.tap'
install-exec-hook:
	$(mkinstalldirs) $(DESTDIR)$(wrapperdir)
	-( cd $(DESTDIR)$(wrapperdir) ; \
		test -f gnome-file-roller.tap \
		|| ln -sf file-roller.tap gnome-file-roller.tap )
	-( cd $(DESTDIR)$(wrapperdir) ; \
		test -f org.gnome.FileRoller.tap \
		|| ln -sf file-roller.tap org.gnome.FileRoller.tap )
# Install symlink to 'mate-engrampa.tap'
	$(mkinstalldirs) $(DESTDIR)$(wrapperdir)
	-( cd $(DESTDIR)$(wrapperdir) ; \
		test -f mate-engrampa.tap \
		|| ln -sf engrampa.tap mate-engrampa.tap )

EXTRA_DIST =								\
	$(wrapper_SCRIPTS)						\
	template.tap

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
