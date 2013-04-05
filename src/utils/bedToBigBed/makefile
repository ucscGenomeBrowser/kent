kentSrc = ../..
A = bedToBigBed
include $(kentSrc)/inc/userApp.mk
L += -lm -lz ${SOCKETLIB}

clean::
	rm -fr tests/output

test::
	@rm -fr tests/output
	@${MKDIR} tests/output
	${A} -type=bed9 -as=itemRgb.as itemRgb.bed chrom.sizes tests/output/itemRgb.bb 2> /dev/null
	${A} tests/output/itemRgb.bb test_itemRgb.bed
	diff itemRgb.bed test_itemRgb.bed
	rm -fr tests/output test_itemRgb.bed
