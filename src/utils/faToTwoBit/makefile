include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = faToTwoBit.o

faToTwoBit: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/faToTwoBit $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/faToTwoBit

clean:
	rm -f $O
