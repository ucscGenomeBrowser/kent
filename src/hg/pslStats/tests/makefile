include ../../../inc/common.mk

test:	untransAlignTest untransQueryTest untransOverallTest \
	untransAlignUnalnTest untransQueryUnalnTest untransOverallUnalnTest

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

# tests with list which has unaligned sequences
untransAlignUnalnTest: mkdirs
	pslStats -queries=input/basic.queries input/basic.psl output/$@.stats
	diff -u expected/$@.stats output/$@.stats

untransQueryUnalnTest: mkdirs
	pslStats -queryStats -queries=input/basic.queries input/basic.psl output/$@.stats
	diff -u expected/$@.stats output/$@.stats

untransOverallUnalnTest: mkdirs
	pslStats -overallStats -queries=input/basic.queries input/basic.psl output/$@.stats
	diff -u expected/$@.stats output/$@.stats

clean::
	rm -rf output

mkdirs:
	@${MKDIR} output
