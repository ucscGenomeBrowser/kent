kentSrc = ../../../..
include ../../../../inc/common.mk

mafToBigMaf = ${DESTBINDIR}/mafToBigMaf
bigMafToMaf = ${DESTBINDIR}/bigMafToMaf
bedToBigBed = ${DESTBINDIR}/bedToBigBed

all:: test

test:: test1

test1: mkout
	${mafToBigMaf} hg38 input/$@.maf output/$@.tmp
	sort -k1,1 -k2,2n output/$@.tmp > output/$@.bigMafInput
	${bedToBigBed} -type=bed3+1 -tab -as=${kentSrc}/hg/lib/bigMaf.as output/$@.bigMafInput input/chrom.sizes output/$@.bb 2> /dev/null
	${bigMafToMaf} output/$@.bb output/$@.maf
	cmp input/$@.maf output/$@.maf

mkout:
	@mkdir -p output

clean::
	rm -rf output
