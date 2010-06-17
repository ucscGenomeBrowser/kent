include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = twinOrfStats.o

twinOrfStats: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/twinOrfStats $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/twinOrfStats${EXE}

clean::
	rm -f ${O}
