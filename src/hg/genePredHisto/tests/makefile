include ../../../inc/common.mk

test: exonLenTest

exonLenTest: mkdirs
	genePredHisto exonLen input/basic.gp output/$@.stats
	diff expected/$@.stats output/$@.stats

mkdirs:
	@mkdir -p output

clean::
	rm -rf output
