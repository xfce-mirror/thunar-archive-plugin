# $Id$

wrapperdir = $(libexecdir)/thunar-archive-plugin
wrapper_SCRIPTS= 						\
	ark.tap								\
	engrampa.tap						\
	file-roller.tap

install-exec-hook:
	$(mkinstalldirs) $(DESTDIR)$(wrapperdir)
# Install symlink to 'gnome-file-roller.tap'
	-( cd $(DESTDIR)$(wrapperdir) ; \
		test -f gnome-file-roller.tap \
		|| ln -sf file-roller.tap gnome-file-roller.tap )
	-( cd $(DESTDIR)$(wrapperdir) ; \
		test -f org.gnome.FileRoller.tap \
		|| ln -sf file-roller.tap org.gnome.FileRoller.tap )
# Install symlink to 'mate-engrampa.tap'
	-( cd $(DESTDIR)$(wrapperdir) ; \
		test -f mate-engrampa.tap \
		|| ln -sf engrampa.tap mate-engrampa.tap )
# Install symlink to 'ark.tap'
	-( cd $(DESTDIR)$(wrapperdir) ; \
		test -f org.kde.ark.tap \
		|| ln -sf ark.tap org.kde.ark.tap )

EXTRA_DIST =								\
	$(wrapper_SCRIPTS)						\
	template.tap

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
