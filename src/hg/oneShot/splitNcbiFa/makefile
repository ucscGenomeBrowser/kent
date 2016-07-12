kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = splitNcbiFa.o

splitNcbiFa: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/splitNcbiFa $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/splitNcbiFa${EXE}

clean::
	rm -f ${O}
