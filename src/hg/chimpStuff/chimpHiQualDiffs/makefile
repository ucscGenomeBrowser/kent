include ../../../inc/common.mk

L = -lm
MYLIBDIR = $(HOME)/kent/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

X = chimpHiQualDiffs
O = ${X}.o

${X}: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/${X} $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/${X}${EXE}
