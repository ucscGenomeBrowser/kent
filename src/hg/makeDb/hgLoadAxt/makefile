include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgLoadAxt.o

hgLoadAxt: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/hgLoadAxt $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/hgLoadAxt${EXE}
