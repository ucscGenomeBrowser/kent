include ../../inc/common.mk

L = -lpthread -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = threadTime.o

threadTime: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/threadTime $O ${MYLIBS} $L
	strip ${BINDIR}/threadTime

clean:
	rm -f $O
