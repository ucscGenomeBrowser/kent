kentSrc = ../../..
A = bigWigMerge
include ../../../inc/common.mk

TESTOUT = output
TESTIN = in
TESTEXPECTED = expected

test: test1

outputDir:
	@${MKDIR} -p ${TESTOUT}

test1: outputDir 
	bedGraphToBigWig ${TESTIN}/t2.bed ${TESTIN}/hg19.chrom.sizes ${TESTOUT}/t2.bw
	bedGraphToBigWig ${TESTIN}/t1.bed ${TESTIN}/hg19.chrom.sizes  ${TESTOUT}/t1.bw
	${A} ${TESTOUT}/t1.bw ${TESTOUT}/t2.bw ${TESTOUT}/12merge.bedGraph -verbose=0
	diff ${TESTOUT}/12merge.bedGraph ${TESTEXPECTED}/12merge.bedGraph 
	${A} ${TESTOUT}/t2.bw ${TESTOUT}/t1.bw ${TESTOUT}/21merge.bedGraph -verbose=0
	diff ${TESTOUT}/21merge.bedGraph ${TESTEXPECTED}/21merge.bedGraph 
	${A} -max ${TESTOUT}/t1.bw ${TESTOUT}/t2.bw ${TESTOUT}/12merge.max.bedGraph -verbose=0
	diff ${TESTOUT}/12merge.max.bedGraph ${TESTEXPECTED}/12merge.max.bedGraph 
	${A} -max ${TESTOUT}/t2.bw ${TESTOUT}/t1.bw ${TESTOUT}/21merge.max.bedGraph -verbose=0
	diff ${TESTOUT}/21merge.max.bedGraph ${TESTEXPECTED}/21merge.max.bedGraph 
	rm -rf ${TESTOUT}

clean:
	rm -rf ${TESTOUT}
