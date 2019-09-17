include ../../../../inc/common.mk

pslRc = ${DESTBINDIR}/pslRc
pslCheck = ${DESTBINDIR}/pslCheck

diff = diff -u

# use pslSwap input files
indir = ../../../pslSwap/tests/input/

test: mrnaTest withSeqTest transTest
	${MAKE} clean

mrnaTest: mkdirs
	${MAKE} runTest ID=$@ IN=mrna.psl

withSeqTest:
	${MAKE} runTest ID=$@ IN=withSeq.psl

transTest:
	${MAKE} runTest ID=$@ IN=trans.psl

# recursive target to run tests
# ID= test id
# IN= file under input
runTest:
	${pslRc} ${indir}/${IN} output/${ID}.psl	
	${pslRc} output/${ID}.psl output/${ID}.back.psl
	${pslCheck} -verbose=0 output/${ID}.psl
	${pslCheck} -verbose=0 output/${ID}.back.psl

mkdirs:
	${MKDIR} output

clean:
	rm -rf output
