include ../../../inc/common.mk

test: untransAlignTest untransQueryTest untransOverallTest

# basic set of tests with an untranslated psl
untransAlignTest: mkdirs
	pslStats input/basic.psl output/$@.stats
	diff -u expected/$@.stats output/$@.stats

untransQueryTest: mkdirs
	pslStats -queryStats input/basic.psl output/$@.stats
	diff -u expected/$@.stats output/$@.stats

untransOverallTest: mkdirs
	pslStats -overallStats input/basic.psl output/$@.stats
	diff -u expected/$@.stats output/$@.stats

clean::
	rm -rf output

mkdirs:
	@${MKDIR} output
