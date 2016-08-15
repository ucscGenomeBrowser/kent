kentSrc = ../../..
include ../../../inc/common.mk

PSLTOBED = ${DESTBINDIR}/pslToBed

test:  doTest1 doTest2

doTest1: mkdirs
	${MAKE} runTest ID=$@ IN=test1.psl.gz

doTest2: mkdirs
	${MAKE} runTestCds ID=$@ IN=test2.psl.gz CDS=test2.cds.gz

# recursive target to run tests
# ID= test id
# IN= file under input
# OPTS= options (optional)
runTestCds:
	${PSLTOBED} -cds=input/${CDS} ${OPTS} input/${IN} output/${ID}.bed
	validateFiles -type=bed12 output/${ID}.bed -chromInfo=input/mm10.chrom.sizes
	zcat expected/${ID}.bed.gz | diff -q /dev/stdin output/${ID}.bed

runTest:
	${PSLTOBED} ${OPTS} input/${IN} output/${ID}.bed
	validateFiles -type=bed12 output/${ID}.bed -chromInfo=input/mm10.chrom.sizes
	zcat expected/${ID}.bed.gz | diff -q /dev/stdin output/${ID}.bed

mkdirs:
	${MKDIR} output

clean:
	rm -rf output
