kentSrc = ../../../..
include ../../../../inc/common.mk
# tests for hgLoadMaf

hgLoadMaf = ${DESTBINDIR}/hgLoadMaf
hgLoadMafSummary = ${DESTBINDIR}/hgLoadMafSummary

#test: testoutput clean
test: testdot testpipe testnameforms testgenarkref testdottedref clean

TEST1 = testNan

testoutput: ${TEST1}

clean:
	rm -fr output

testdot: init-tests
	${hgLoadMafSummary} -test hg38 output/testDot input/testDot.maf
	diff output/testDot.tab expected

testpipe: init-tests
	${hgLoadMafSummary} -test hg38 output/testPipe input/testPipe.maf
	diff output/testPipe.tab expected

# every sequence-name form mafSplitSrcGetChrom handles, in one maf
testnameforms: init-tests
	${hgLoadMafSummary} -test hg38 output/testNameForms input/testNameForms.maf
	diff output/testNameForms.tab expected

# GenArk accession as the reference assembly: chrom must not keep the version dot
testgenarkref: init-tests
	${hgLoadMafSummary} -test GCF_000001405.40 output/testGenArkRef input/testGenArkRef.maf
	diff output/testGenArkRef.tab expected

# dotted reference assembly that is not a GCA_/GCF_ accession, so only the rule that
# matches the given database can split it: without that rule the chrom comes out as
# "1.contig7" instead of "contig7"
testdottedref: init-tests
	${hgLoadMafSummary} -test AABBCC010000001.1 output/testDottedRef input/testDottedRef.maf
	diff output/testDottedRef.tab expected

testNan: init-tests
	${hgLoadMaf} -warn -test=input/${TEST1}.maf hg16 ${TEST1} > output/${TEST1}.log
	mv ${TEST1}.tab output
	diff output/${TEST1}.tab expected
	diff output/${TEST1}.log expected

init-tests:
	${MKDIR} output

