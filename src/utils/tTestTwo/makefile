include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = tTestTwo.o

tTestTwo: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/tTestTwo $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/tTestTwo

clean:
	rm -f $O
