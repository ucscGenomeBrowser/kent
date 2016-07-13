kentSrc = ../../..
include ../../../inc/common.mk


L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = checkNt.o

checkNt: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/checkNt $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/checkNt${EXE}

clean::
	rm -f ${O}
