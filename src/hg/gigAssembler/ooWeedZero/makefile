kentSrc = ../../..
include ../../../inc/common.mk


L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = ooWeedZero.o

ooWeedZero: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/ooWeedZero $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/ooWeedZero${EXE}

clean::
	rm -f ${O}
