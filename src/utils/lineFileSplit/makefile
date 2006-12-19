include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = lineFileSplit.o

lineFileSplit: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/lineFileSplit $O ${MYLIBS} $L
	strip ${BINDIR}/lineFileSplit

clean:
	rm -f $O
