include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = twinOrfStats.o

twinOrfStats: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/twinOrfStats $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/twinOrfStats${EXE}
