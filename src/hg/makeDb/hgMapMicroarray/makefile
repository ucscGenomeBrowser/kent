include ../../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = hgMapMicroarray.o

hgMapMicroarray: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/hgMapMicroarray $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/hgMapMicroarray${EXE}

clean:
	rm -f $O
