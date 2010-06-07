include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = splitFileByColumn.o

splitFileByColumn: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/splitFileByColumn $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/splitFileByColumn${EXE}

clean:
	rm -f $O
