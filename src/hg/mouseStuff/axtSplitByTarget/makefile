include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = axtSplitByTarget.o

axtSplitByTarget: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/axtSplitByTarget $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/axtSplitByTarget${EXE}
