include ../../../../inc/common.mk

all:

test: rnaBindingClusterCorrTest rnaBindingClusterCorrAntiTest \
   altProbesBrainSpTest


#	altHtmlPagesSjRatioTest altHtmlPagesSjRatioLog2Test \

# test files: 
# clusterTest.tab - List of splice junctions in a fake cluster.
# psIntenTest.tab - Intensities for gene probe sets.
# rnaBinderTest.tab - List of fake RNA binding genes to compare to cluster.
# sjIndexTest.tab - Splice junction indexes for sj's in cluster.

altProbesBrainSpTest: mkout
	altProbes -junctionProbes=input/ndr2PSets.bed -probFile=input/altProbesNdr2PSets.pval \
	   -db=mm2 -intensityFile=input/altProbsNdr2PSets.intensity \
	   -spliceFile=input/altProbesNdr2CassEvent.splice -brainSpecific=bs.clean >& tmp.txt
	diff -u tmp.txt expected/Ndr2BsSpecific.txt
	rm tmp.txt

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

altHtmlPagesSjRatioTest:
	altHtmlPages -junctFile=input/altHtmlSjRatioJs.tab -intensityFile=input/altHtmlSjInten.tab  \
	    -ratioFile=output/sjRatios.tab -doSjRatios >& /dev/null
	diff -u expected/altHtmlSjRatio.tab output/sjRatios.tab 

altHtmlPagesSjRatioLog2Test:
	altHtmlPages -junctFile=input/altHtmlSjRatioJs.tab -intensityFile=input/altHtmlSjInten.tab  \
	    -ratioFile=output/sjRatios.tab -doSjRatios -log2Ratio >& /dev/null
	diff -u expected/altHtmlSjRatioLog2.tab output/sjRatios.tab 

mkout:
	${MKDIR} output
clean:
	rm -rf output
