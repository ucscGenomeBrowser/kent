include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = hgMapMicroarray.o

hgMapMicroarray: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/hgMapMicroarray $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/hgMapMicroarray${EXE}

clean:
	rm -f $O
