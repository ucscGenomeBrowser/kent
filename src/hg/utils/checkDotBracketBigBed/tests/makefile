kentSrc = ../../../..
include ${kentSrc}/inc/common.mk

# the program finds bigBedInfo and bigBedToBed on PATH, so point that at the same
# build the tests use to make the files
export PATH := ${DESTBINDIR}:${PATH}

checkDotBracketBigBed = ${DESTBINDIR}/checkDotBracketBigBed
bedToBigBed = ${DESTBINDIR}/bedToBigBed

# -tab is required building these: the strings are space padded
buildBlocks = ${bedToBigBed} -tab -sort -as=input/dotBracket.as -type=bed12+1

test:: blocksTest noBlocksTest badTest midColumnTest noColumnTest
	${MAKE} clean

# bed12 with blocks on both strands, pseudoknot brackets and space padding, all valid
blocksTest: mkout
	${buildBlocks} input/blocks.bed input/chrom.sizes output/$@.bb
	${checkDotBracketBigBed} output/$@.bb dotBracket > output/$@.out
	diff expected/$@.out output/$@.out

# blockless bed6, real hg19 evofold rows, where the span is the length to match
noBlocksTest: mkout
	${bedToBigBed} -tab -sort -as=input/dotBracketNoBlocks.as -type=bed6+1 \
		input/noBlocks.bed input/chrom.sizes output/$@.bb
	${checkDotBracketBigBed} output/$@.bb dotBracket > output/$@.out
	diff expected/$@.out output/$@.out

# one row per kind of problem; the ! is because a file with problems exits non-zero
badTest: mkout
	${buildBlocks} input/bad.bed input/chrom.sizes output/$@.bb
	! ${checkDotBracketBigBed} output/$@.bb dotBracket > output/$@.out 2> output/$@.err
	diff expected/$@.out output/$@.out
	diff expected/$@.err output/$@.err

# the dot-bracket column is neither the last column nor a fixed bed one, which is what
# the column index arithmetic gets wrong when it is wrong
midColumnTest: mkout
	${bedToBigBed} -tab -sort -as=input/dotBracketMid.as -type=bed12+3 \
		input/midColumn.bed input/chrom.sizes output/$@.bb
	! ${checkDotBracketBigBed} output/$@.bb dotBracket > output/$@.out 2> output/$@.err
	diff expected/$@.out output/$@.out
	diff expected/$@.err output/$@.err

# a column the bigBed does not have, which raises rather than listing it as bad data
noColumnTest: mkout
	${buildBlocks} input/blocks.bed input/chrom.sizes output/$@.bb
	${checkDotBracketBigBed} output/$@.bb notAColumn 2>&1 | grep '^Exception:' > output/$@.out
	diff expected/$@.out output/$@.out

mkout:
	@mkdir -p output

clean::
	rm -rf output
