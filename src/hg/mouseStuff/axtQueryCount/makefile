include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = axtQueryCount.o

axtQueryCount: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/axtQueryCount $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/axtQueryCount${EXE}
