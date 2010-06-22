include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 


O = hgLoadRnaFold.o

hgLoadRnaFold: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/hgLoadRnaFold $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/hgLoadRnaFold${EXE}

test:
	hgLoadRnaFold -tab=output -noLoad hg16 test input
	diff -x CVS -r expected output
	rm -r output

clean::
	rm -f ${O}
