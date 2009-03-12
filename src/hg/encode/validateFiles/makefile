include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a ${MYLIBDIR}/jkweb.a

O = validateFiles.o

validateFiles: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/validateFiles $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/validateFiles${EXE}

clean:
	rm -f $O
