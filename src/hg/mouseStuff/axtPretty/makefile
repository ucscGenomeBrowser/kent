include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = axtPretty.o

axtPretty: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/axtPretty $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/axtPretty${EXE}
