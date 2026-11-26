kentSrc = ../../..
include ../../../inc/common.mk

# featureBits checks that every BED and bigBed item fits on its chromosome,
# and stops with "Coordinate out of allowed range" instead of writing past the
# end of its bit array.  refs #38780
#
# Each case runs featureBits on a 1000-base chrT (-chromSize, so no database is
# needed) and records the last line of output and the exit status.  An error
# line is cut at ": item", because the item's coordinates are not the point:
# the #38773 code clamps a huge end to INT_MAX before the check, so the same
# overflow item prints a different end there.  The in-range
# cases are the positive check: they must still count bases.  overflow.bed has a
# block start plus a block size that wraps a 32-bit int; with 32-bit arithmetic
# the check passes and featureBits crashes.
#
# To test another binary:  make test featureBits=/path/to/featureBits

featureBits = ${DESTBINDIR}/featureBits
bedToBigBed = ${DESTBINDIR}/bedToBigBed
FB = ${featureBits} test -countGaps -chromSize=input/chrom.sizes

all:

test: rangeTest messageTest

rangeTest: mkdirs
	${bedToBigBed} -verbose=0 input/inRangeBlocks.bed input/bigBed.sizes output/inRange.bb
	${bedToBigBed} -verbose=0 input/bigBedPastEnd.bed input/bigBed.sizes output/pastEnd.bb
	rm -f output/$@
	@for c in "input/inRange.bed" \
	          "input/inRangeBlocks.bed" \
	          "input/inRangeBlocks.bed -countBlocks" \
	          "input/onePast.bed" \
	          "input/allPast.bed" \
	          "input/blockPast.bed" \
	          "input/blockPast.bed -countBlocks" \
	          "input/negBlock.bed -countBlocks" \
	          "input/overflow.bed" \
	          "input/overflow.bed -countBlocks" \
	          "output/inRange.bb" \
	          "output/inRange.bb -countBlocks" \
	          "output/pastEnd.bb" ; do \
	    echo "## $$c" >> output/$@ ; \
	    ${FB} $$c > output/out.txt 2>&1 ; echo "exit $$?" > output/exit.txt ; \
	    tail -1 output/out.txt | sed "s/: item .*//" >> output/$@ ; cat output/exit.txt >> output/$@ ; \
	done
	diff -u expected/$@ output/$@

# The full error line: the item's real coordinates (not clamped to INT_MAX) and its line
# number.  An empty BED file counts nothing and is not an error.
messageTest: mkdirs
	rm -f output/$@
	@for c in "input/overflow.bed -countBlocks" \
	          "input/allPast.bed" \
	          "input/empty.bed" ; do \
	    echo "## $$c" >> output/$@ ; \
	    ${FB} $$c > output/out.txt 2>&1 ; echo "exit $$?" > output/exit.txt ; \
	    tail -1 output/out.txt >> output/$@ ; cat output/exit.txt >> output/$@ ; \
	done
	diff -u expected/$@ output/$@

clean::
	rm -rf output

mkdirs:
	@${MKDIR} output
