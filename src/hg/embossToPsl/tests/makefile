include ../../../inc/common.mk

all:

test: baseTest twoTest


# one alignment, sizes on command line
baseTest: mkdirs
	../embossToPsl -qSize=2180 -tSize=6591 input/base.pair output/baseTest.psl
	pslCheck output/baseTest.psl
	diff -u expected/baseTest.psl output/baseTest.psl

# two alignments, sizes in files
twoTest: mkdirs
	../embossToPsl -qSizes=input/two.qsizes -tSizes=input/two.tsizes input/two.pair output/twoTest.psl
	pslCheck output/twoTest.psl
	diff -u expected/twoTest.psl output/twoTest.psl


clean::
	rm -rf output

mkdirs:
	@mkdir -p output
