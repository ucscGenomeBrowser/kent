include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = twinOrfStats3.o

twinOrfStats3: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/twinOrfStats3 $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/twinOrfStats3${EXE}
