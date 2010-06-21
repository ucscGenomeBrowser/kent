include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = raToCds.o

raToCds: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/raToCds $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/raToCds${EXE}

clean::
	rm -f ${O}
