include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = phToPsl.o

phToPsl: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/phToPsl $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/phToPsl${EXE}

clean::
	rm -f ${O}
