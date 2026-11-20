kentSrc = ../../../..
include ${kentSrc}/inc/common.mk

pslToBigPsl = ${DESTBINDIR}/pslToBigPsl
bigPslToPsl = ${DESTBINDIR}/bigPslToPsl
bedToBigBed = ${DESTBINDIR}/bedToBigBed

# sh commands to deal with test that should have failed and didn't 
cmdShouldFail = echo "Error: command should have failed" >&2; false

all:: test

test:: singleStrand doubleStrand badQSize protOverlap protOverlapTrim
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

badQSize: mkout
	if ! ${pslToBigPsl} -fa=input/MamRep605b.fa input/MamRep605b.psl output/$@.tmp >& output/$@.err ; then true ; else ${cmdShouldFail} ; fi

# a translated alignment joining blocks in two reading frames: the target blocks overlap,
# which BED has no way to write, so the default is to stop
protOverlap: mkout
	if ! ${pslToBigPsl} input/$@.psl output/$@.tmp >& output/$@.err ; then true ; else ${cmdShouldFail} ; fi

# the same input with -trimOverlap: the first block is cut from 60 to 14 codons so that it
# ends before the second one starts, and bedToBigBed then takes the file
protOverlapTrim: mkout
	${pslToBigPsl} -trimOverlap input/protOverlap.psl output/$@.bigPsl
	diff expected/$@.bigPsl output/$@.bigPsl
	sort -k1,1 -k2,2n output/$@.bigPsl > output/$@.bigPslInput
	${bedToBigBed} -type=bed12+12 -tab -as=${kentSrc}/hg/lib/bigPsl.as output/$@.bigPslInput input/chrom.sizes output/$@.bb 2> /dev/null

mkout:
	@mkdir -p output

clean::
	rm -rf output
