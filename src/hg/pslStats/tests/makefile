include ../../../inc/common.mk

test: basicTests

# basic set of tests with an untranslated psl
basicTests: mkdirs
	pslStats input/basic.psl output/basic.alnStats output/basic.queryStats
	diff -u expected/basic.alnStats output/basic.alnStats
	diff -u expected/basic.queryStats output/basic.queryStats

clean::
	rm -rf output

mkdirs:
	@${MKDIR} output
