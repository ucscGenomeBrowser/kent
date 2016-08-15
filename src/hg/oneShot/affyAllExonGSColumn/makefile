kentSrc = ../../..
include ../../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a ${MYLIBDIR}/jkweb.a

O = affyAllExonGSColumn.o

affyAllExonGSColumn: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/affyAllExonGSColumn $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/affyAllExonGSColumn${EXE}

clean:
	rm -f $O
