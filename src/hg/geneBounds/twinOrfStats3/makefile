include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = twinOrfStats3.o

twinOrfStats3: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/twinOrfStats3 $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/twinOrfStats3${EXE}

clean::
	rm -f ${O}
