include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = axtDropSelf.o

axtDropSelf: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/axtDropSelf $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/axtDropSelf${EXE}
