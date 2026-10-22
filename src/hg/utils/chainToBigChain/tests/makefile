kentSrc = ../../../..
include ../../../../inc/common.mk

chainToBigChain = ${DESTBINDIR}/chainToBigChain
bigChainToChain = ${DESTBINDIR}/bigChainToChain
bedToBigBed = ${DESTBINDIR}/bedToBigBed
bigBedToBed = ${DESTBINDIR}/bigBedToBed
chainSort = ${DESTBINDIR}/chainSort

all:: test

test:: simpleTest overlapTest

simpleTest: mkout
	${chainToBigChain} input/hg38.mm10.test.chain output/$@.bigChain output/$@.bigChainLink
	diff expected/$@.bigChain output/$@.bigChain
	diff expected/$@.bigChainLink output/$@.bigChainLink
	${bedToBigBed} -type=bed6+6 -as=${kentSrc}/hg/lib/bigChain.as -tab output/$@.bigChain input/hg38.chrom.sizes output/$@.bb
	${bedToBigBed} -type=bed4+1 -as=${kentSrc}/hg/lib/bigLink.as -tab output/$@.bigChainLink input/hg38.chrom.sizes output/$@.link.bb
	${bigBedToBed} output/$@.bb output/$@.bb.bed
	diff expected/$@.bigChain output/$@.bb.bed
	${bigBedToBed} output/$@.link.bb output/$@.link.bb.bed
	diff expected/$@.bigChainLink output/$@.link.bb.bed
	${bigChainToChain} output/$@.bb output/$@.link.bb output/$@.test.chain
	${chainSort} output/$@.test.chain output/$@.test.chain2
	diff input/hg38.mm10.test.chain output/$@.test.chain2

# Six chains that overlap on one target chrom, so their links interleave in the link
# file, with ids out of order (7, 3, 1234567, 42, 5, 900) and both query strands.
# bigChainToChain reads them back through chainLoadIdRangeHub(), which hands each chain
# the links of its own id, in link-file order.  The input is already chainSort'ed.  refs #38591
overlapTest: mkout
	${chainToBigChain} input/overlap.chain output/$@.bigChain output/$@.bigChainLink
	${bedToBigBed} -type=bed6+6 -as=${kentSrc}/hg/lib/bigChain.as -tab output/$@.bigChain input/chrT.sizes output/$@.bb
	${bedToBigBed} -type=bed4+1 -as=${kentSrc}/hg/lib/bigLink.as -tab output/$@.bigChainLink input/chrT.sizes output/$@.link.bb
	${bigChainToChain} output/$@.bb output/$@.link.bb output/$@.test.chain
	${chainSort} output/$@.test.chain output/$@.test.chain2
	diff input/overlap.chain output/$@.test.chain2

mkout:
	@mkdir -p output

clean::
	rm -rf output
