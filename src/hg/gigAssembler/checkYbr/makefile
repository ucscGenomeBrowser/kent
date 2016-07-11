kentSrc = ../../..
include ../../../inc/common.mk


L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = checkYbr.o

checkYbr: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/checkYbr $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/checkYbr${EXE}

clean::
	rm -f ${O}
