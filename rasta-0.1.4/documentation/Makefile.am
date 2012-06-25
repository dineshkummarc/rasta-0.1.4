
SUBDIRS = man

LYX_SOURCE =		\
	reference.lyx	\
	whitepaper.lyx

TEX_SOURCE =		\
	reference.tex	\
	whitepaper.tex

EXTRA_DIST =		\
	$(LYX_SOURCE)	\
	$(TEX_SOURCE)

reference: reference.tex
	$(LATEX2HTML) -local_icons $<

whitepaper: whitepaper.tex
	$(LATEX2HTML) -local_icons $<

docs: whitepaper reference

