include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

A = linearSat
O = linearSat.o

linearSat: ${O} ${MYLIBS}
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS} $L
#       ${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

compile:: ${O}
	${CC} ${COPT} -o ${A}${EXE} ${O} ${MYLIBS} $L

clean::
	rm -f ${A}${EXE} ${O}

test:
	linearSat yReads.txt yTypeOrder.txt testY.out chain=testY.before afterChain=testY.after outSize=10000 seed=0 verbose=1
	linearSat 3Reads.txt 3TypeOrder.txt test3.out chain=test3.before afterChain=test3.after outSize=17543 seed=0 verbose=1

