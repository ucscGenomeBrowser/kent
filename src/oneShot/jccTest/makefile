include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = jccTest.o

jccTest: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/jccTest $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/jccTest

clean:
	rm -f $O
