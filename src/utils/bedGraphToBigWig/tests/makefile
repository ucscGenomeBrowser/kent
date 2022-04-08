kentSrc = ../../..
A = bedGraphToBigWig
include ../../../inc/common.mk

test: testBadChrom1 testBbSize1 testBbSize2

testBadChrom1:  outputDir
	-${BINDIR}/${A} input/test1.bed  input/human.chrom.sizes.txt output/noFile.chromAlias.bb   2> output/testBadChrom1.err
	diff expected/testBadChrom1.err output/testBadChrom1.err

output/human.chromAlias.bb:  outputDir
	bedToBigBed input/human.chromAlias.bed input/human.chrom.sizes.txt output/human.chromAlias.bb   -tab -type=bed3+4 -as=input/chromAliasIndex.as -extraIndex=genbank,ensembl,ncbi,ucsc

testBbSize1: outputDir output/human.chromAlias.bb
	${BINDIR}/${A} input/test1.bed output/human.chromAlias.bb output/test1.bw -sizesIsBb 
	bigWigToBedGraph output/test1.bw output/test1.bed
	diff input/test1.bed output/test1.bed

testBbSize2: outputDir output/human.chromAlias.bb
	-${BINDIR}/${A} input/mixed.bed output/human.chromAlias.bb output/noFile.chromAlias.bb -sizesIsBb 2> output/mixed.err
	diff expected/mixed.err output/mixed.err

clean::
	@rm -rf output

outputDir:
	@${MKDIR} -p output
