include ../../../../inc/common.mk

all:

test: rnaBindingClusterCorrTest rnaBindingClusterCorrAntiTest

# test files: 
# clusterTest.tab - List of splice junctions in a fake cluster.
# psIntenTest.tab - Intensities for gene probe sets.
# rnaBinderTest.tab - List of fake RNA binding genes to compare to cluster.
# sjIndexTest.tab - Splice junction indexes for sj's in cluster.

rnaBindingClusterCorrTest: mkout
	rnaBindingClusterCorr -sjIndexFile=input/sjIndexTest.tab -psFile=input/psIntenTest.tab \
	   -clusterFile=input/clusterTest.tab -rnaBindingFile=input/rnaBinderTest.tab \
	   -outputFile=output/test.out.tab >& /dev/null
	diff -u expected/rnaBindingClusterTestOutput.corr.tab output/test.out.tab

rnaBindingClusterCorrAntiTest: mkout
	rnaBindingClusterCorr -sjIndexFile=input/sjIndexTest.tab -psFile=input/psIntenTest.tab \
	   -clusterFile=input/clusterTest.tab -rnaBindingFile=input/rnaBinderTest.tab \
	   -outputFile=output/test.out.tab -antiCorrelation >& /dev/null
	diff -u expected/rnaBindingClusterTestOutput.antiCorr.tab output/test.out.tab

mkout:
	${MKDIR} output
clean:
	rm -rf output
