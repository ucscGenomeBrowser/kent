include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = twinOrf2.o

twinOrf2: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/twinOrf2 $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/twinOrf2${EXE}

clean::
	rm -f ${O}
