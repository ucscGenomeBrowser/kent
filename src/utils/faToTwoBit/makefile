include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = faToTwoBit.o

faToTwoBit: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/faToTwoBit $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/faToTwoBit${EXE}

clean:
	rm -f $O

test:
	${MKDIR} tests/output
	faToTwoBit tests/input/testN.fa tests/output/testN.2bit
	twoBitToFa tests/output/testN.2bit tests/output/testN.fa
	diff tests/input/testN.fa tests/output/testN.fa
	faToTwoBit tests/input/testMask.fa tests/output/testMask.2bit
	twoBitToFa tests/output/testMask.2bit tests/output/testMask.fa
	diff tests/input/testMask.fa tests/output/testMask.fa
#	rm -r tests/output
