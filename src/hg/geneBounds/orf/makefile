include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = orf.o

orf: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/orf $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/orf${EXE}

clean::
	rm -f ${O}
