.c.o:
	gcc -ggdb -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../inc -I../../inc -c $*.c

L = /projects/cc/hg/jk/outSrc/expat/lib/.libs/libexpat.a -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = autoXml.o

autoXml: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/autoXml $O $(MYLIBS) $L
#	strip $(HOME)/bin/$(MACHTYPE)/autoXml
