include ../../../inc/common.mk

PSLSWAP = pslSwap

test: mrnaTest withSeqTest transTest mrnaNoRcTest withSeqNoRcTest

mrnaTest: mkdirs
	${MAKE} runTest ID=$@ IN=mrna.psl

withSeqTest:
	${MAKE} runTest ID=$@ IN=withSeq.psl

transTest:
	${MAKE} runTest ID=$@ IN=trans.psl

mrnaNoRcTest: mkdirs
	${MAKE} runNoRcTest ID=$@ IN=mrna.psl

withSeqNoRcTest:
	${MAKE} runNoRcTest ID=$@ IN=withSeq.psl

# recursive target to run tests
# ID= test id
# IN= file under input
# OPTS= options (optional)
runTest:
	${PSLSWAP} ${OPTS} input/${IN} output/${ID}.psl	
	${PSLSWAP} ${OPTS} output/${ID}.psl output/${ID}.back.psl
	pslCheck -verbose=0 output/${ID}.psl
	pslCheck -verbose=0 output/${ID}.back.psl
	diff expected/${ID}.psl output/${ID}.psl
	diff input/${IN} output/${ID}.back.psl

# recursive target to run -noRc tests.  Since target strand is added,
# we must remove it on the swap-back part of the test
# ID= test id
# IN= file under input
# OPTS= options (optional)
runNoRcTest:
	${PSLSWAP} -noRc ${OPTS} input/${IN} output/${ID}.psl	
	${PSLSWAP} -noRc ${OPTS} output/${ID}.psl output/${ID}.back.psl
	pslCheck -verbose=0 output/${ID}.psl
	pslCheck -verbose=0 output/${ID}.back.psl
	awk 'BEGIN{FS="\t"; OFS="\t"} {$$9=substr($$9,1,1); print $$0}' output/${ID}.back.psl >output/${ID}.back.fix.psl
	diff expected/${ID}.psl output/${ID}.psl
	diff input/${IN} output/${ID}.back.fix.psl

mkdirs:
	${MKDIR} output

clean:
	rm -rf output
