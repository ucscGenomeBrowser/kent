kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = blastzPaper.o

blastzPaper: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/blastzPaper $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/blastzPaper${EXE}

clean::
	rm -f ${O}
