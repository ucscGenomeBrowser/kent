include ../../../inc/common.mk

all:

test: base

# basic set of tests
base: mkdirs
	mrnaToGene -cdsFile=input/base.cds input/base.psl output/base.gp
	diff -u expected/base.gp output/base.gp

clean::
	rm -rf output

mkdirs:
	@${MKDIR} output
