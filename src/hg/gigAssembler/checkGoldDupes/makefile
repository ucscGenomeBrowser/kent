kentSrc = ../../..
include ../../../inc/common.mk


L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = checkGoldDupes.o

checkGoldDupes: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/checkGoldDupes $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/checkGoldDupes${EXE}

clean::
	rm -f ${O}
