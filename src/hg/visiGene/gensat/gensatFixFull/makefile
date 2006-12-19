include ../../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = gensatFixFull.o

gensatFixFull: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/gensatFixFull $O ${MYLIBS} $L
	strip ${BINDIR}/gensatFixFull

clean:
	rm -f $O
