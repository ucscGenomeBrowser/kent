include ../../inc/common.mk

L = -lm ${MYSQLLIBS}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

hgTracksRandom: hgTracksRandom.o ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${HOME}/bin/${MACHTYPE}/hgTracksRandom hgTracksRandom.o ${MYLIBS} $L

rangeQuery: rangeQuery.o ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${HOME}/bin/${MACHTYPE}/rangeQuery rangeQuery.o ${MYLIBS} $L

compile: hgTracksRandom.o rangeQuery.o ${MYLIBS}
	${CC} -o hgTracksRandom hgTracksRandom.o ${MYLIBS} $L
	${CC} -o rangeQuery rangeQuery.o ${MYLIBS} $L


