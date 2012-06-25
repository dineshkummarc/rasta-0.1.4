
SUBDIRS =		\
	librasta	\
	clrasta		\
	gtkrasta	\
	$(CGIRASTA)	\
	tools		\
	documentation	\
	tests		\
	examples

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = rasta-$(LT_RELEASE).pc

rastadtddir = $(DTD_LOCATION)
rastadtd_DATA =			\
	rasta.dtd		\
	rastamodify.dtd		\
	rastastate.dtd		\
	rastacgiconf.dtd	\
	rastacgiuser.dtd	\
	rastacgiauth.dtd	\
	rastacgisession.dtd

DEBIAN_FILES =					\
	debian/README.Debian			\
	debian/cgirasta.copyright		\
	debian/cgirasta.files			\
	debian/cgirasta.manpages		\
	debian/changelog			\
	debian/clrasta.copyright		\
	debian/clrasta.files			\
	debian/clrasta.manpages			\
	debian/compat				\
	debian/control				\
	debian/gtkrasta.copyright		\
	debian/gtkrasta.files			\
	debian/gtkrasta.manpages		\
	debian/librasta-dev.copyright		\
	debian/librasta-dev.files		\
	debian/librasta.copyright		\
	debian/librasta.files			\
	debian/postinst				\
	debian/prerm				\
	debian/rasta-doc.copyright		\
	debian/rasta-doc.docs			\
	debian/rasta-tools.copyright		\
	debian/rasta-tools.files		\
	debian/rasta-tools.manpages		\
	debian/rules

REDHAT_FILES =					\
	redhat/rasta.spec.in

EXTRA_DIST =				\
	system.rasta.in			\
	rasta-$(LT_RELEASE).pc.in	\
	${rastadtd_DATA}		\
	${DEBIAN_FILES}			\
	${REDHAT_FILES}

docs:
	cd documentation && $(MAKE) docs


