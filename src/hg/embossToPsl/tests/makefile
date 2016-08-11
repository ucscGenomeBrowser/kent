kentSrc = ../../..
include ../../../inc/common.mk

embossToPsl=${DESTBINDIR}/embossToPsl
pslCheck=${DESTBINDIR}/pslCheck

all:

test: baseTest twoTest


# one alignment, sizes on command line
baseTest: mkdirs
	${embossToPsl} -qSizes='U53442 2180' -tSizes='chr22.f0 6591' input/base.pair output/baseTest.psl
	${pslCheck} -verbose=0 output/baseTest.psl
	diff -u expected/baseTest.psl output/baseTest.psl

# two alignments, sizes in files
twoTest: mkdirs
	${embossToPsl} -qSizesFile=input/two.qsizes -tSizesFile=input/two.tsizes input/two.pair output/twoTest.psl
	${pslCheck} -verbose=0 output/twoTest.psl
	diff -u expected/twoTest.psl output/twoTest.psl


clean::
	rm -rf output

mkdirs:
	@${MKDIR} output
