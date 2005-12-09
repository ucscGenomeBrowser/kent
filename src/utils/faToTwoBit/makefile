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
	faToTwoBit tests/input/noDna.fa tests/output/noDna.2bit
	twoBitToFa tests/output/noDna.2bit tests/output/noDna.fa
	diff tests/input/noDna.fa tests/output/noDna.fa
	faToTwoBit tests/input/testN.fa tests/output/testN.2bit
	twoBitToFa tests/output/testN.2bit tests/output/testN.fa
	diff tests/input/testN.fa tests/output/testN.fa
	faToTwoBit tests/input/testMask.fa tests/output/testMask.2bit
	twoBitToFa tests/output/testMask.2bit tests/output/testMask.fa
	diff tests/input/testMask.fa tests/output/testMask.fa
	faToTwoBit -stripVersion tests/input/genbank.fa tests/output/genbank.2bit
	twoBitToFa tests/output/genbank.2bit tests/output/genbank.strip.fa
	diff tests/expected/genbank.fa tests/output/genbank.strip.fa
#	rm -r tests/output
