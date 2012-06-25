
lib_LTLIBRARIES = librasta.la

INCLUDES =			\
	@GLIB_CFLAGS@		\
	@XML_CFLAGS@

librasta_la_LDFLAGS =						\
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)	\
	@GLIB_LIBS@						\
	@XML_LIBS@

librastaincludedir = $(includedir)/rasta-$(LT_RELEASE)
librastainclude_HEADERS =		\
	rasta.h	

librasta_la_private_headers =	\
	rastaaction.h		\
	rastacontext.h		\
	rastadialog.h		\
	rastahidden.h		\
	rastaexec.h		\
	rastamenu.h		\
	rastascope.h		\
	rastascreen.h		\
	rastatraverse.h

librasta_la_SOURCES =		\
	rastaaction.c		\
	rastacontext.c		\
	rastadialog.c		\
	rastahidden.c		\
	rastainitcommand.c	\
	rastaexec.c		\
	rastamenu.c		\
	rastascope.c		\
	rastascreen.c		\
	rastatraverse.c		\
	renumeration.c

man_MANS = 

EXTRA_DIST =				\
	$(librasta_la_private_headers)

