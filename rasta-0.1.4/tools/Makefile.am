
bin_PROGRAMS = rastaedit rastaadd rastadel

INCLUDES =				\
	-I${top_srcdir}/librasta	\
	@GTK_CFLAGS@			\
	@GLIB_CFLAGS@			\
	@XML_CFLAGS@


# Skipped for now
# -ltimbo	-ldb			
rastaedit_LDADD =			\
	../librasta/librasta.la		\
	@GTK_LIBS@			\
	@GLIB_LIBS@			\
	@XML_LIBS@

rastaedit_SOURCES =		\
	rastaedit.c		\
	gtkrastaliststore.h	\
	gtkrastaliststore.c

rastaadd_LDADD =			\
	../librasta/librasta.la		\
	@GLIB_LIBS@			\
	@XML_LIBS@

rastaadd_SOURCES =		\
	rastaadd.c		\
	radcommon.h		\
	radcommon.c

rastadel_LDADD =			\
	../librasta/librasta.la		\
	@GLIB_LIBS@			\
	@XML_LIBS@

rastadel_SOURCES =		\
	rastadel.c		\
	radcommon.h		\
	radcommon.c

man_MANS = 

EXTRA_DIST =

