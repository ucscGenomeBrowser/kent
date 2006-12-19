include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = twoBitInfo.o

twoBitInfo: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/twoBitInfo $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/twoBitInfo${EXE}

clean:
	rm -f $O

# reuse twoFitToFa input files. for tests
TEST_IN = ../twoBitToFa/tests/input

test:
	${MKDIR} tests/output
	twoBitInfo ${TEST_IN}/testN.2bit tests/output/testN.info
	twoBitInfo ${TEST_IN}/testMask.2bit tests/output/testMask.info
	twoBitInfo ${TEST_IN}/testMask.2bit:manyLower tests/output/ml.info
	twoBitInfo ${TEST_IN}/testMask.2bit:manyLower:2-10 tests/output/ml_sub.info
	twoBitInfo ${TEST_IN}/testMask.2bit:noLower,startLower tests/output/ml_multiple.info
	diff -r -x CVS tests/expected tests/output
	rm -r tests/output
