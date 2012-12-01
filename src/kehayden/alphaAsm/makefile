include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

A = alphaAsm
O = alphaAsm.o

alphaAsm: ${O} ${MYLIBS}
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS} $L
#       ${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

compile:: ${O}
	${CC} ${COPT} -o ${A}${EXE} ${O} ${MYLIBS} $L

clean::
	rm -f ${A}${EXE} ${O}

test:
	alphaAsm yReads.txt yTypeOrder.txt testY.out chain=testY.before afterChain=testY.after outSize=10000 seed=0 verbose=1
	alphaAsm 3Reads.txt 3TypeOrder.txt test3.out chain=test3.before afterChain=test3.after outSize=17543 seed=0 verbose=1

