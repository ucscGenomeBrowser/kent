include ../../../inc/common.mk

test: exonSizeHistoTest

exonSizeHistoTest: mkdirs
	genePredStats exonSizeHisto input/basic.gp output/$@.stats
	diff expected/$@.stats output/$@.stats

mkdirs:
	@mkdir -p output

clean::
	rm -rf output
