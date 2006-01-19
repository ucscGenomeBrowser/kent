include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = hgKgGetText.o

hgKgGetText: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/hgKgGetText $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/hgKgGetText

clean:
	rm -f $O
