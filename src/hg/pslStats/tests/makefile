kentSrc = ../../..
include ../../../inc/common.mk

pslStats = ${DESTBINDIR}/pslStats

test:	untransAlignTest untransQueryTest untransOverallTest \
	untransAlignUnalnTest untransQueryUnalnTest untransOverallUnalnTest \
	cdna2cdnaAlignTest cdna2cdnaQueryTest cdna2cdnaOverallTest

# basic set of tests with an untranslated psl
untransAlignTest: mkdirs
	${pslStats} input/basic.psl output/$@.stats
	diff -u expected/$@.stats output/$@.stats

untransQueryTest: mkdirs
	${pslStats} -queryStats input/basic.psl output/$@.stats
	diff -u expected/$@.stats output/$@.stats

untransOverallTest: mkdirs
	${pslStats} -overallStats input/basic.psl output/$@.stats
	diff -u expected/$@.stats output/$@.stats

# tests with list which has unaligned sequences
untransAlignUnalnTest: mkdirs
	${pslStats} -queries=input/basic.queries input/basic.psl output/$@.stats
	diff -u expected/$@.stats output/$@.stats

untransQueryUnalnTest: mkdirs
	${pslStats} -queryStats -queries=input/basic.queries input/basic.psl output/$@.stats
	diff -u expected/$@.stats output/$@.stats

untransOverallUnalnTest: mkdirs
	${pslStats} -overallStats -queries=input/basic.queries input/basic.psl output/$@.stats
	diff -u expected/$@.stats output/$@.stats

# cDNA to cDNA alignments, will have tCover that is interesting
cdna2cdnaAlignTest: mkdirs
	${pslStats} -queries=input/cdna2cdna.queries input/cdna2cdna.psl output/$@.stats
	diff -u expected/$@.stats output/$@.stats

cdna2cdnaQueryTest: mkdirs
	${pslStats} -queryStats -queries=input/cdna2cdna.queries input/cdna2cdna.psl output/$@.stats
	diff -u expected/$@.stats output/$@.stats

cdna2cdnaOverallTest: mkdirs
	${pslStats} -overallStats -queries=input/cdna2cdna.queries input/cdna2cdna.psl output/$@.stats
	diff -u expected/$@.stats output/$@.stats

clean::
	rm -rf output

mkdirs:
	@${MKDIR} output
