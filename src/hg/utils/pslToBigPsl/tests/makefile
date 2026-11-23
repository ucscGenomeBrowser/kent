kentSrc = ../../../..
include ${kentSrc}/inc/common.mk

pslToBigPsl = ${DESTBINDIR}/pslToBigPsl
bigPslToPsl = ${DESTBINDIR}/bigPslToPsl
bedToBigBed = ${DESTBINDIR}/bedToBigBed

# sh commands to deal with test that should have failed and didn't 
cmdShouldFail = echo "Error: command should have failed" >&2; false

all:: test

test:: singleStrand doubleStrand badQSize protOverlap protOverlapTrim outOfOrder
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
# ends before the second one starts, and bedToBigBed then takes the file.  The third record
# is the first one mirrored onto the target minus strand, where the query starts of the
# blocks count from the other end of the query; the query range of the trimmed alignment is
# still reported on the forward query, so both records report 526-1122
protOverlapTrim: mkout
	${pslToBigPsl} -trimOverlap input/protOverlap.psl output/$@.bigPsl
	diff expected/$@.bigPsl output/$@.bigPsl
	sort -k1,1 -k2,2n output/$@.bigPsl > output/$@.bigPslInput
	${bedToBigBed} -type=bed12+12 -tab -as=${kentSrc}/hg/lib/bigPsl.as output/$@.bigPslInput input/chrom.sizes output/$@.bb 2> /dev/null

# target blocks that do not ascend: trimming cannot make these fit, so stop rather than write
# a row that bedToBigBed would reject
outOfOrder: mkout
	if ! ${pslToBigPsl} -trimOverlap input/$@.psl output/$@.tmp >& output/$@.err ; then true ; else ${cmdShouldFail} ; fi

mkout:
	@mkdir -p output

clean::
	rm -rf output
