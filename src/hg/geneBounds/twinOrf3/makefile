include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = twinOrf3.o

twinOrf3: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/twinOrf3 $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/twinOrf3${EXE}

clean::
	rm -f ${O}
