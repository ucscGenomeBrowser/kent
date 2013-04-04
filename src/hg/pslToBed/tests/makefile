include ../../../inc/common.mk

PSLTOBED = pslToBed

test:  doTest1 doTest2

doTest1: mkdirs
	${MAKE} runTest ID=$@ IN=test1.psl

doTest2: mkdirs
	${MAKE} runTestCds ID=$@ IN=test2.psl CDS=test2.cds

# recursive target to run tests
# ID= test id
# IN= file under input
# OPTS= options (optional)
runTestCds:
	${PSLTOBED} -cds=input/${CDS} ${OPTS} input/${IN} output/${ID}.bed
	validateFiles -type=bed12 output/${ID}.bed -chromInfo=input/mm10.chrom.sizes
	diff expected/${ID}.bed output/${ID}.bed

runTest:
	${PSLTOBED} ${OPTS} input/${IN} output/${ID}.bed
	validateFiles -type=bed12 output/${ID}.bed -chromInfo=input/mm10.chrom.sizes
	diff expected/${ID}.bed output/${ID}.bed

mkdirs:
	${MKDIR} output

clean:
	rm -rf output
