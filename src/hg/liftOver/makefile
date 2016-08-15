kentSrc = ../..
include ../../inc/common.mk


L += ${MYSQLLIBS} -lm 
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = liftOver
O = liftOver.o

all: liftOver liftOverMerge

liftOver: $O ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS} $L
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

liftOverMerge:  liftOverMerge.o ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${DESTDIR}${BINDIR}/liftOverMerge liftOverMerge.o ${MYLIBS} $L
	${STRIP} ${DESTDIR}${BINDIR}/liftOverMerge${EXE}

compile:: ${O} liftOverMerge.o
	${CC} ${COPT} ${CFLAGS} -o ${A} $O ${MYLIBS} $L
	${CC} ${COPT} ${CFLAGS} -o liftOverMerge liftOverMerge.o ${MYLIBS} $L

clean::
	rm -f ${A} ${O} liftOverMerge liftOverMerge.o

test::
	cd tests && ${MAKE} test
