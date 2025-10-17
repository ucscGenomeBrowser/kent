kentSrc = ../../..
A = bedToBigBed
include ../../../inc/common.mk

test: testOddSorted testRgb testMultiInsAtEnd itemsRgb tabSep testBadChrom1 testBbSize1 testBbSize2 testDevStdin testStdin \
	testCompress testNotSorted testFixScores

# use ! cmd 2> output/$@.err || (${shouldHaveFailed})
shouldHaveFailed = echo "Error: command should have failed" >&2 && false

fakeGenomeTwoBit = output/fakeGenome.2bit

testBadChrom1:  outputDir
	! ${BINDIR}/${A} input/colored.genbank.bed input/human.chrom.sizes.txt output/noFile.chromAlias.bb -tab -type=bed12+13 -as=input/bigGenePred.as 2> output/testBadChrom1.err || (${shouldHaveFailed})
	diff expected/testBadChrom1.err output/testBadChrom1.err

output/human.chromAlias.bb: outputDir
	${BINDIR}/${A} input/human.chromAlias.bed input/human.chrom.sizes.txt output/human.chromAlias.bb   -tab -type=bed3+4 -as=input/chromAliasIndex.as -extraIndex=genbank,ensembl,ncbi,ucsc

testBbSize1: outputDir output/human.chromAlias.bb
	${BINDIR}/${A} input/colored.genbank.bed output/human.chromAlias.bb output/colored.genbank.bb -sizesIsChromAliasBb -tab -type=bed12+13 -as=input/bigGenePred.as 
	bigBedToBed output/colored.genbank.bb output/colored.genbank.bed
	diff input/colored.genbank.bed output/colored.genbank.bed

testBbSize2: outputDir output/human.chromAlias.bb
	! ${BINDIR}/${A} input/mixed.bed output/human.chromAlias.bb output/noFile.chromAlias.bb -sizesIsChromAliasBb -tab -type=bed12+13 -as=input/bigGenePred.as 2> output/mixed.err || (${shouldHaveFailed})
	diff expected/mixed.err output/mixed.err

testRgb: outputDir
	${BINDIR}/${A} -type=bed9 -as=input/itemRgb.as input/itemRgb.bed input/chrom.sizes output/itemRgb.bb 2> /dev/null
	bigBedToBed output/itemRgb.bb output/test_itemRgb.bed
	diff input/itemRgb.bed output/test_itemRgb.bed

testCompress: outputDir
	${BINDIR}/${A} -type=bed9 -as=input/itemRgb.as input/itemRgb.bed.gz input/chrom.sizes output/itemRgbGz.bb 2> /dev/null
	bigBedToBed output/itemRgbGz.bb output/test_itemRgbGz.bed
	diff input/itemRgb.bed output/test_itemRgbGz.bed

testOddSorted: outputDir
	${BINDIR}/${A} -extraIndex=name -type=bed12+16 -tab -as=../../..//hg/lib/gencodeBGP.as input/oddSorted.bed input/hg38.chrom.sizes output/oddSorted.bb
	bigBedToBed output/oddSorted.bb output/oddSorted.bed
	LC_ALL=C sort -k1,1 -k2,2n  input/oddSorted.bed > output/oddSorted.check.bed
	diff output/oddSorted.check.bed output/oddSorted.bed
	cut -f 4 input/oddSorted.bed > output/oddSorted.names
	bigBedNamedItems output/oddSorted.bb output/oddSorted.names -nameFile stdout | LC_ALL=C sort -k1,1 -k2,2n >  output/searchNames.bed
	diff output/oddSorted.bed output/searchNames.bed

testSortNotSorted: outputDir
	${BINDIR}/${A} -tab -type=bed9+3 -sort input/notSorted.bed input/notSorted.chrom.sizes output/notSorted.bb 
	bigBedToBed output/notSorted.bb output/notSorted.bed
	diff expected/notSorted.bed output/notSorted.bed

testNotSorted: outputDir
	! ${BINDIR}/${A} -type=bed12 input/notSorted.bed input/notSorted.chrom.sizes /dev/null 2> output/notSorted.err || (${shouldHaveFailed})
	diff expected/notSorted.err output/notSorted.err

testMultiInsAtEnd: outputDir
	${BINDIR}/${A} input/multiInsAtEnd.bed input/chrom.sizes output/multiInsAtEnd.bb 2> /dev/null
	bigBedToBed output/multiInsAtEnd.bb output/test_multiInsAtEnd.bed
	diff -w input/multiInsAtEnd.bed output/test_multiInsAtEnd.bed

itemsRgb: outputDir
	${BINDIR}/${A} input/reservedBed12.bed input/chrom.sizes output/reservedBed12.bb 2> /dev/null
	bigBedToBed output/reservedBed12.bb output/test_reservedBed12.bed
	diff input/reservedBed12.bed output/test_reservedBed12.bed
	${BINDIR}/${A} -type=bed12 input/reservedBed12.bed input/chrom.sizes output/reservedBed12.bb 2> /dev/null
	bigBedToBed output/reservedBed12.bb output/test_reservedBed12.bed
	diff input/reservedBed12.bed output/test_reservedBed12.bed
	${BINDIR}/${A} -type=bed9+ -as=input/reservedBed12.as input/reservedBed12.bed input/chrom.sizes output/reservedBed12.bb 2> /dev/null
	bigBedToBed output/reservedBed12.bb output/test_reservedBed12.bed
	diff input/reservedBed12.bed output/test_reservedBed12.bed
	${BINDIR}/${A} -type=bed9+ -as=input/itemRgbBed12.as input/reservedBed12.bed input/chrom.sizes output/reservedBed12.bb 2> /dev/null
	bigBedToBed output/reservedBed12.bb output/test_reservedBed12.bed
	diff input/reservedBed12.bed output/test_reservedBed12.bed
	${BINDIR}/${A} input/itemRgbBed12.bed input/chrom.sizes output/itemRgbBed12.bb 2> /dev/null
	bigBedToBed output/itemRgbBed12.bb output/test_itemRgbBed12.bed
	diff input/itemRgbBed12.bed output/test_itemRgbBed12.bed
	${BINDIR}/${A} -type=bed12 input/itemRgbBed12.bed input/chrom.sizes output/itemRgbBed12.bb 2> /dev/null
	bigBedToBed output/itemRgbBed12.bb output/test_itemRgbBed12.bed
	diff input/itemRgbBed12.bed output/test_itemRgbBed12.bed
	${BINDIR}/${A} -type=bed12 -as=input/reservedBed12.as input/itemRgbBed12.bed input/chrom.sizes output/itemRgbBed12.bb 2> /dev/null
	bigBedToBed output/itemRgbBed12.bb output/test_itemRgbBed12.bed
	diff input/itemRgbBed12.bed output/test_itemRgbBed12.bed
	${BINDIR}/${A} -type=bed12 -as=input/itemRgbBed12.as input/itemRgbBed12.bed input/chrom.sizes output/itemRgbBed12.bb 2> /dev/null
	bigBedToBed output/itemRgbBed12.bb output/test_itemRgbBed12.bed
	diff input/itemRgbBed12.bed output/test_itemRgbBed12.bed

tabSep: outputDir
	${BINDIR}/${A} -tab -type=bed4 -as=input/tabSep.as input/tabSep.bed input/chrom.sizes output/tabSep.bb 2> /dev/null
	bigBedToBed output/tabSep.bb output/test_tabSep.bed
	diff input/tabSep.bed output/test_tabSep.bed

testDevStdin: outputDir
	cat  input/itemRgb.bed | (! ${BINDIR}/${A} -type=bed9 -as=input/itemRgb.as /dev/stdin input/chrom.sizes output/$@.bb 2> output/$@.err) || (${shouldHaveFailed})
	diff expected/$@.err output/$@.err

testStdin: outputDir
	cat input/itemRgb.bed | (! ${BINDIR}/${A} -type=bed9 -as=input/itemRgb.as stdin input/chrom.sizes output/$@.bb 2> output/$@.err) || (${shouldHaveFailed})
	diff expected/$@.err output/$@.err

test2bitMissingSeq: outputDir ${fakeGenomeTwoBit}
	! ${BINDIR}/${A} -tab -type=bed4 -as=input/tabSep.as -sizesIs2Bit input/tabSep.bed ${fakeGenomeTwoBit} output/tabSep.bb 2> output/$@.err || (${shouldHaveFailed})
	diff expected/$@.err output/$@.err

testFixScores: outputDir
	${BINDIR}/${A} -fixScores -verbose=0 -type=bed9 -as=input/itemRgb.as input/badScores.bed input/chrom.sizes output/fixScores.bb
	bigBedToBed output/fixScores.bb output/fixScores.bed
	diff expected/fixScores.bed output/fixScores.bed


${fakeGenomeTwoBit}: outputDir input/fakeGenome.fa
	faToTwoBit input/fakeGenome.fa $@ 

clean::
	@rm -rf output

outputDir:
	@${MKDIR} -p output
