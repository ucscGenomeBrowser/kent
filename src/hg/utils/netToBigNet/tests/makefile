kentSrc = ../../../..
include ../../../../inc/common.mk

netToBigNet = ${DESTBINDIR}/netToBigNet
bedToBigBed = ${DESTBINDIR}/bedToBigBed

all:: test

test:: simpleTest

simpleTest: mkout
	${netToBigNet} input/hg38.mm39.test.net output/$@.bigNet
	diff expected/$@.bigNet output/$@.bigNet
	${bedToBigBed} -type=bed6+20 -as=${kentSrc}/hg/lib/bigNet.as -tab output/$@.bigNet input/hg38.chrom.sizes output/$@.bb
	bigBedToBed output/$@.bb output/$@.bb.bed
	diff expected/$@.bigNet output/$@.bb.bed

mkout:
	@mkdir -p output

clean::
	rm -rf output
