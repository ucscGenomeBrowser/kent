kentSrc = ../../../..
include ../../../../inc/common.mk

pslToBigPsl = ${DESTBINDIR}/pslToBigPsl
bigPslToPsl = ${DESTBINDIR}/bigPslToPsl
bedToBigBed = ${DESTBINDIR}/bedToBigBed

# sh commands to deal with gff3ToPsl shouldhave failed and didn't 
cmdShouldFail = echo "Error: command should have failed" >&2; false

all:: test

test:: singleStrand doubleStrand
	${MAKE} clean

singleStrand: mkout
	${pslToBigPsl} input/$@.psl output/$@.tmp
	sort -k1,1 -k2,2n output/$@.tmp > output/$@.bigPslInput
	${bedToBigBed} -type=bed12+12 -tab -as=${kentSrc}/hg/lib/bigPsl.as output/$@.bigPslInput input/chrom.sizes output/$@.bb 2> /dev/null
	${bigPslToPsl} -collapseStrand output/$@.bb output/$@.psl
	cmp input/$@.psl output/$@.psl

doubleStrand: mkout
	${pslToBigPsl} input/$@.psl output/$@.tmp
	sort -k1,1 -k2,2n output/$@.tmp > output/$@.bigPslInput
	${bedToBigBed} -type=bed12+12 -tab -as=${kentSrc}/hg/lib/bigPsl.as output/$@.bigPslInput input/chrom.sizes output/$@.bb 2> /dev/null
	${bigPslToPsl} output/$@.bb output/$@.psl
	cmp input/$@.psl output/$@.psl

mkout:
	@mkdir -p output

clean::
	rm -rf output
