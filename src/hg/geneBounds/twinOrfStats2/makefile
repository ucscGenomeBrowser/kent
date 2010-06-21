include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = twinOrfStats2.o

twinOrfStats2: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/twinOrfStats2 $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/twinOrfStats2${EXE}

clean::
	rm -f ${O}
