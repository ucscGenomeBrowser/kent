kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = dupeFoo.o

dupeFoo: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/dupeFoo $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/dupeFoo${EXE}

clean::
	rm -f ${O}
