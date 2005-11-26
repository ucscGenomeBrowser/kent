include ../../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = gensatFixFull.o

gensatFixFull: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/gensatFixFull $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/gensatFixFull

clean:
	rm -f $O
