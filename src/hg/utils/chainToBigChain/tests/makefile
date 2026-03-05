kentSrc = ../../../..
include ../../../../inc/common.mk

chainToBigChain = ${DESTBINDIR}/chainToBigChain
bedToBigBed = ${DESTBINDIR}/bedToBigBed

all:: test

test:: simpleTest

simpleTest: mkout
	${chainToBigChain} input/hg38.mm10.test.chain output/$@.bigChain output/$@.bigChainLink
	diff expected/$@.bigChain output/$@.bigChain
	diff expected/$@.bigChainLink output/$@.bigChainLink
	bedToBigBed -type=bed6+6 -as=${kentSrc}/hg/lib/bigChain.as -tab output/$@.bigChain input/hg38.chrom.sizes output/$@.bb
	bedToBigBed -type=bed4+1 -as=${kentSrc}/hg/lib/bigLink.as -tab output/$@.bigChainLink input/hg38.chrom.sizes output/$@.link.bb
	bigBedToBed output/$@.bb output/$@.bb.bed
	diff expected/$@.bigChain output/$@.bb.bed
	bigBedToBed output/$@.link.bb output/$@.link.bb.bed
	diff expected/$@.bigChainLink output/$@.link.bb.bed
	bigChainToChain output/$@.bb output/$@.link.bb output/$@.test.chain
	chainSort output/$@.test.chain output/$@.test.chain2
	diff input/hg38.mm10.test.chain output/$@.test.chain2

mkout:
	@mkdir -p output

clean::
	rm -rf output
