include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = plateOfPrimers.o

plateOfPrimers: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/plateOfPrimers $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/plateOfPrimers

clean:
	rm -f $O
