kentSrc = ../../..
A = ${DESTBINDIR}/bigWigCat 
include ../../../inc/common.mk

test: outputDir
	# generate big wigs
	${DESTBINDIR}/wigToBigWig -fixedSummaries -keepAllChromosomes input/testFixed_A.wig input/test.sizes tmp/testFixed_A.bigWig
	${DESTBINDIR}/wigToBigWig -fixedSummaries -keepAllChromosomes input/testFixed_B.wig input/test.sizes tmp/testFixed_B.bigWig
	${A} tmp/testFixed.bigWig tmp/testFixed_*.bigWig

	${DESTBINDIR}/wigToBigWig -fixedSummaries -keepAllChromosomes input/testVar_A.wig input/test.sizes tmp/testVar_A.bigWig
	${DESTBINDIR}/wigToBigWig -fixedSummaries -keepAllChromosomes input/testVar_B.wig input/test.sizes tmp/testVar_B.bigWig
	${A} tmp/testVar.bigWig tmp/testVar_[AB].bigWig

	${DESTBINDIR}/wigToBigWig -fixedSummaries -keepAllChromosomes input/testBedGraph_A.wig input/test.sizes tmp/testBedGraph_A.bigWig
	${DESTBINDIR}/wigToBigWig -fixedSummaries -keepAllChromosomes input/testBedGraph_B.wig input/test.sizes tmp/testBedGraph_B.bigWig
	${A} tmp/testBedGraph.bigWig tmp/testBedGraph_[AB].bigWig

	# get unscaled data back out
	bigWigToBedGraph tmp/testFixed.bigWig output/testFixed.bedGraph
	bigWigToBedGraph tmp/testVar.bigWig output/testVar.bedGraph
	bigWigToBedGraph tmp/testBedGraph.bigWig output/testBedGraph.bedGraph

	# generate summaries on testFixed at lots of resolutions
	bigWigSummary tmp/testFixed.bigWig chr7 115597761 115598761 1 > output/tf1.sum
	bigWigSummary tmp/testFixed.bigWig chr7 115597761 115598761 3 > output/tf3.sum
	bigWigSummary tmp/testFixed.bigWig chr7 115597761 115598761 10 > output/tf10.sum
	bigWigSummary tmp/testFixed.bigWig chr7 115597761 115598761 30 > output/tf30.sum
	bigWigSummary tmp/testFixed.bigWig chr7 115597761 115598761 100 > output/tf100.sum
	bigWigSummary tmp/testFixed.bigWig chr7 115597761 115598761 300 > output/tf300.sum
	bigWigSummary tmp/testFixed.bigWig chr7 115597761 115598761 1000 > output/tf1000.sum
	bigWigSummary tmp/testFixed.bigWig chr7 115597761 115598761 3000 > output/tf3000.sum
	bigWigSummary tmp/testFixed.bigWig chr7 115597761 115598761 10000 > output/tf10000.sum

	# generate summaries on testVar at 5 resolutions
	bigWigSummary tmp/testVar.bigWig chrY 81000 91000 1 > output/tv1.sum
	bigWigSummary tmp/testVar.bigWig chrY 81000 91000 10 > output/tv10.sum
	bigWigSummary tmp/testVar.bigWig chrY 81000 91000 100 > output/tv100.sum
	bigWigSummary tmp/testVar.bigWig chrY 81000 91000 1000 > output/tv1000.sum
	bigWigSummary tmp/testVar.bigWig chrY 81000 91000 10000 > output/tv10000.sum

	# generate summaries on testBedGraph at 5 resolutions
	bigWigSummary tmp/testBedGraph.bigWig chr5 131308806 132230652 1 > output/tbg1.sum
	bigWigSummary tmp/testBedGraph.bigWig chr5 131308806 132230652 10 > output/tbg10.sum
	bigWigSummary tmp/testBedGraph.bigWig chr5 131308806 132230652 100 > output/tbg100.sum
	bigWigSummary tmp/testBedGraph.bigWig chr5 131308806 132230652 1000 > output/tbg1000.sum
	bigWigSummary tmp/testBedGraph.bigWig chr5 131308806 132230652 10000 > output/tbg10000.sum

	# Do the diff
	diff output expected

	# clean up temporary directories
	rm -r output tmp

clean::
	@rm -rf output
	@rm -rf tmp 

outputDir:
	@${MKDIR} -p output
	@${MKDIR} -p tmp
