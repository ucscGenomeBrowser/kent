kentSrc = ../..
A = bigWigMerge
include $(kentSrc)/inc/userApp.mk
L += -lm -lz

TESTOUT = test/output
TESTIN = test/in
TESTEXPECTED = test/expected

clean::
	rm -rf ${TESTOUT}

tests:: ${A}
	mkdir -p ${TESTOUT}
	bedGraphToBigWig ${TESTIN}/t2.bed ${TESTIN}/hg19.chrom.sizes ${TESTOUT}/t2.bw
	bedGraphToBigWig ${TESTIN}/t1.bed ${TESTIN}/hg19.chrom.sizes  ${TESTOUT}/t1.bw
	${A} ${TESTOUT}/t1.bw ${TESTOUT}/t2.bw ${TESTOUT}/12merge.bedGraph
	diff ${TESTOUT}/12merge.bedGraph ${TESTEXPECTED}/12merge.bedGraph 
	${A} ${TESTOUT}/t2.bw ${TESTOUT}/t1.bw ${TESTOUT}/21merge.bedGraph
	diff ${TESTOUT}/21merge.bedGraph ${TESTEXPECTED}/21merge.bedGraph 
	rm -rf ${TESTOUT}
