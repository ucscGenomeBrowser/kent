kentSrc = ../../../..
include ../../../../inc/common.mk

eseScorer=${DESTBINDIR}/eseScorer
rnaBindingClusterCorr=${DESTBINDIR}/rnaBindingClusterCorr
altProbes=${DESTBINDIR}/altProbes
altHtmlPages=${DESTBINDIR}/altHtmlPages

all:

test: eseScorerTest rnaBindingClusterCorrTest rnaBindingClusterCorrAntiTest altProbesTest

eseScorerTest: mkout
	${eseScorer} input/eseScorerTest.score input/eseScorerTest.fa output/eseScorerTest.result
	diff output/eseScorerTest.result expected/eseScorerTest.gold.out

#	altHtmlPagesSjRatioTest altHtmlPagesSjRatioLog2Test \

# test files: 
# clusterTest.tab - List of splice junctions in a fake cluster.
# psIntenTest.tab - Intensities for gene probe sets.
# rnaBinderTest.tab - List of fake RNA binding genes to compare to cluster.
# sjIndexTest.tab - Splice junction indexes for sj's in cluster.
altProbesTest: 
	${altProbes} -doTests

rnaBindingClusterCorrTest: mkout
	${rnaBindingClusterCorr} -sjIndexFile=input/sjIndexTest.tab -psFile=input/psIntenTest.tab \
	   -clusterFile=input/clusterTest.tab -rnaBindingFile=input/rnaBinderTest.tab \
	   -outputFile=output/test.out.tab >& /dev/null
	diff -u expected/rnaBindingClusterTestOutput.corr.tab output/test.out.tab

rnaBindingClusterCorrAntiTest: mkout
	${rnaBindingClusterCorr} -sjIndexFile=input/sjIndexTest.tab -psFile=input/psIntenTest.tab \
	   -clusterFile=input/clusterTest.tab -rnaBindingFile=input/rnaBinderTest.tab \
	   -outputFile=output/test.out.tab -antiCorrelation >& /dev/null
	diff -u expected/rnaBindingClusterTestOutput.antiCorr.tab output/test.out.tab

altHtmlPagesSjRatioTest:
	${altHtmlPages} -junctFile=input/altHtmlSjRatioJs.tab -intensityFile=input/altHtmlSjInten.tab  \
	    -ratioFile=output/sjRatios.tab -doSjRatios >& /dev/null
	diff -u expected/altHtmlSjRatio.tab output/sjRatios.tab 

altHtmlPagesSjRatioLog2Test:
	${altHtmlPages} -junctFile=input/altHtmlSjRatioJs.tab -intensityFile=input/altHtmlSjInten.tab  \
	    -ratioFile=output/sjRatios.tab -doSjRatios -log2Ratio >& /dev/null
	diff -u expected/altHtmlSjRatioLog2.tab output/sjRatios.tab 

mkout:
	${MKDIR} output
clean:
	rm -rf output
