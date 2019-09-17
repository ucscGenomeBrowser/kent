kentSrc = ../../..
A = bedToBigBed
include ../../../inc/common.mk

test: outputDir testRgb testNotSorted testMultiInsAtEnd clean

testRgb:
	${BINDIR}/${A} -type=bed9 -as=input/itemRgb.as input/itemRgb.bed input/chrom.sizes output/itemRgb.bb 2> /dev/null
	bigBedToBed output/itemRgb.bb output/test_itemRgb.bed
	diff input/itemRgb.bed output/test_itemRgb.bed

testNotSorted:
	-${BINDIR}/${A} -type=bed12 input/notSorted.bed input/notSorted.chrom.sizes /dev/null 2> output/notSorted.err || true
	diff expected/notSorted.err output/notSorted.err

testMultiInsAtEnd:
	${BINDIR}/${A} input/multiInsAtEnd.bed input/chrom.sizes output/multiInsAtEnd.bb 2> /dev/null
	bigBedToBed output/multiInsAtEnd.bb output/test_multiInsAtEnd.bed
	diff -w input/multiInsAtEnd.bed output/test_multiInsAtEnd.bed

clean::
	@rm -rf output

outputDir:
	@${MKDIR} -p output
