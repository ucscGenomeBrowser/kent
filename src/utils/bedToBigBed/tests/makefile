A = bedToBigBed
include ../../../inc/common.mk

test: outputDir
	${A} -type=bed9 -as=input/itemRgb.as input/itemRgb.bed input/chrom.sizes output/itemRgb.bb 2> /dev/null
	bigBedToBed output/itemRgb.bb output/test_itemRgb.bed
	diff input/itemRgb.bed output/test_itemRgb.bed

clean::
	@rm -rf output

outputDir:
	@${MKDIR} -p output
