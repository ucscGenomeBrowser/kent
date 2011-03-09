include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a 
CFLAGS= -g

O = protDat.o

protDat: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/protDat $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/protDat${EXE}

clean::
	rm -f ${O}
