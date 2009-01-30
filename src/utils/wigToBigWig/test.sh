#!/bin/tcsh -ef
# Relatively quick test script for bigWig system
# Output should be:
#      Only in expected: CVS
# And nothing more

# make temporary directories
mkdir -p testOut
mkdir -p testTmp

# generate big wigs
wigToBigWig testFixed.wig test.sizes testTmp/testFixed.bigWig
wigToBigWig testVar.wig test.sizes testTmp/testVar.bigWig
wigToBigWig testBedGraph.wig test.sizes testTmp/testBedGraph.bigWig

# get unscaled data back out
bigWigToBedGraph testTmp/testFixed.bigWig testOut/testFixed.bedGraph
bigWigToBedGraph testTmp/testVar.bigWig testOut/testVar.bedGraph
bigWigToBedGraph testTmp/testBedGraph.bigWig testOut/testBedGraph.bedGraph

# generate summaries on testFixed at lots of resolutions
bigWigSummary testTmp/testFixed.bigWig chr7 115597761 115598761 1 > testOut/tf1.sum
bigWigSummary testTmp/testFixed.bigWig chr7 115597761 115598761 3 > testOut/tf3.sum
bigWigSummary testTmp/testFixed.bigWig chr7 115597761 115598761 10 > testOut/tf10.sum
bigWigSummary testTmp/testFixed.bigWig chr7 115597761 115598761 30 > testOut/tf30.sum
bigWigSummary testTmp/testFixed.bigWig chr7 115597761 115598761 100 > testOut/tf100.sum
bigWigSummary testTmp/testFixed.bigWig chr7 115597761 115598761 300 > testOut/tf300.sum
bigWigSummary testTmp/testFixed.bigWig chr7 115597761 115598761 1000 > testOut/tf1000.sum
bigWigSummary testTmp/testFixed.bigWig chr7 115597761 115598761 3000 > testOut/tf3000.sum
bigWigSummary testTmp/testFixed.bigWig chr7 115597761 115598761 10000 > testOut/tf10000.sum

# generate summaries on testVar at 5 resolutions
bigWigSummary testTmp/testVar.bigWig chrY 81000 91000 1 > testOut/tv1.sum
bigWigSummary testTmp/testVar.bigWig chrY 81000 91000 10 > testOut/tv10.sum
bigWigSummary testTmp/testVar.bigWig chrY 81000 91000 100 > testOut/tv100.sum
bigWigSummary testTmp/testVar.bigWig chrY 81000 91000 1000 > testOut/tv1000.sum
bigWigSummary testTmp/testVar.bigWig chrY 81000 91000 10000 > testOut/tv10000.sum

# generate summaries on testBedGraph at 5 resolutions
bigWigSummary testTmp/testBedGraph.bigWig chr5 131308806 132230652 1 > tbg1.sum
bigWigSummary testTmp/testBedGraph.bigWig chr5 131308806 132230652 10 > tbg10.sum
bigWigSummary testTmp/testBedGraph.bigWig chr5 131308806 132230652 100 > tbg100.sum
bigWigSummary testTmp/testBedGraph.bigWig chr5 131308806 132230652 1000 > tbg1000.sum
bigWigSummary testTmp/testBedGraph.bigWig chr5 131308806 132230652 10000 > tbg10000.sum

# Do the diff - only output of script should be about the expected/CVS dir
diff testOut expected

# clean up temporary directories (not executed since fails on diff because of CVS)
rm -r testOut testTmp
