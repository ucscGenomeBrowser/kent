include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

X = chimpHiQualDiffs
O = ${X}.o

${X}: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/${X} $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/${X}${EXE}

clean::
	rm -f ${O}
