kentSrc = ../../..
# Run a couple of simple sanity check tests.
include ../../../inc/common.mk

faRandomize=${DESTBINDIR}/faRandomize

test: freqTest noGTest

mkdirs: 
	@${MKDIR} output

freqTest: mkdirs
	@${faRandomize} input/freqTest.fa output/freqTest.fa -seed=10 2> /dev/null
	@diff expected/freqTest.gold.fa output/freqTest.fa 1>&2

noGTest: mkdirs
	@${faRandomize} input/noGTest.fa output/noGTest.fa -seed=10 2> /dev/null
	@diff expected/noGTest.gold.fa output/noGTest.fa 1>&2

clean: 
	rm -rf output

