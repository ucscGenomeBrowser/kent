kentSrc = ../../../..
include ../../../../inc/common.mk

mafToBigMafSummary = ${DESTBINDIR}/mafToBigMafSummary

test: testdot testpipe testnameforms testgenarkref testdottedref clean

testdot: init
	${mafToBigMafSummary} hg38 input/testDot.maf output/testDot.bed
	diff expected/testDot.bed output/testDot.bed

testpipe: init
	${mafToBigMafSummary} hg38 input/testPipe.maf output/testPipe.bed
	diff expected/testPipe.bed output/testPipe.bed

# every sequence-name form mafSplitSrcGetChrom handles, in one maf
testnameforms: init
	${mafToBigMafSummary} hg38 input/testNameForms.maf output/testNameForms.bed
	diff expected/testNameForms.bed output/testNameForms.bed

# GenArk accession as the reference assembly: chrom must not keep the version dot
testgenarkref: init
	${mafToBigMafSummary} GCF_000001405.40 input/testGenArkRef.maf output/testGenArkRef.bed
	diff expected/testGenArkRef.bed output/testGenArkRef.bed

# dotted reference assembly that is not a GCA_/GCF_ accession, so only the rule that
# matches the given database can split it: without that rule the chrom comes out as
# "1.contig7" instead of "contig7"
testdottedref: init
	${mafToBigMafSummary} AABBCC010000001.1 input/testDottedRef.maf output/testDottedRef.bed
	diff expected/testDottedRef.bed output/testDottedRef.bed

init:
	${MKDIR} output

clean:
	rm -fr output
