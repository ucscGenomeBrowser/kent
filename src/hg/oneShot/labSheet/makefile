include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = labSheet.o

labSheet: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/labSheet $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/labSheet

clean:
	rm -f $O
