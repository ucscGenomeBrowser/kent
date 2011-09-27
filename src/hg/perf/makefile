include ../../inc/common.mk

L += -lm ${MYSQLLIBS}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = hgTracksRandom.o

hgTracksRandom: ${O} ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/hgTracksRandom hgTracksRandom.o ${MYLIBS} $L

rangeQuery: rangeQuery.o ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/rangeQuery rangeQuery.o ${MYLIBS} $L

compile: hgTracksRandom.o rangeQuery.o ${MYLIBS}
	${CC} -o hgTracksRandom hgTracksRandom.o ${MYLIBS} $L
	${CC} -o rangeQuery rangeQuery.o ${MYLIBS} $L

clean::
	rm -f ${O} rangeQuery.o rangeQuery hgTracksRandom
