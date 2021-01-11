kentSrc = ../../..
A = bedToBigBed
include ../../../inc/common.mk

test: testRgb testNotSorted testMultiInsAtEnd itemsRgb

testRgb: outputDir
	${BINDIR}/${A} -type=bed9 -as=input/itemRgb.as input/itemRgb.bed input/chrom.sizes output/itemRgb.bb 2> /dev/null
	bigBedToBed output/itemRgb.bb output/test_itemRgb.bed
	diff input/itemRgb.bed output/test_itemRgb.bed

testNotSorted: outputDir
	-${BINDIR}/${A} -type=bed12 input/notSorted.bed input/notSorted.chrom.sizes /dev/null 2> output/notSorted.err || true
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

clean::
	@rm -rf output

outputDir:
	@${MKDIR} -p output
