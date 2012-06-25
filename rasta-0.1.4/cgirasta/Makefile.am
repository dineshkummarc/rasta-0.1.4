
bin_PROGRAMS = cgirasta

INCLUDES =				\
	-I${top_srcdir}/librasta	\
	@GLIB_CFLAGS@			\
	@XML_CFLAGS@


cgirasta_LDADD =			\
	../librasta/librasta.la		\
	@GLIB_LIBS@			\
	@XML_LIBS@			\
	@CRYPT_LIBS@

cgirasta_SOURCES =		\
	cgirasta.c		\
	jsha.c			\
	jcgi.c			\
	jiterator.c

cgirastadefaulttemplatesdir = $(DTD_LOCATION)/templates
cgirastadefaulttemplates_DATA =	\
	error.html		\
	login.html		\
	screen.html		\
	action.html

cgirasta_private_headers =			\
	jsha.h					\
	jcgi.h					\
	jiterator.h

man_MANS = 

EXTRA_DIST =					\
	$(cgirasta_private_headers)		\
	$(cgirastadefaulttemplates_DATA)

