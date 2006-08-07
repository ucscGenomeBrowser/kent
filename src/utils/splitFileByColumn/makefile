include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = splitFileByColumn.o

splitFileByColumn: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/splitFileByColumn $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/splitFileByColumn${EXE}

clean:
	rm -f $O
