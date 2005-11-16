include ../../../inc/common.mk

all:

test: base frame allCds header

# basic set of tests
base: mkdirs
	mrnaToGene -cdsFile=input/base.cds input/base.psl output/$@.gp
	genePredCheck output/$@.gp
	diff -u expected/$@.gp output/$@.gp

# test with frame info
frame: mkdirs
	mrnaToGene -genePredExt -cdsFile=input/base.cds input/base.psl output/$@.gp
	genePredCheck output/$@.gp
	diff -u expected/$@.gp output/$@.gp

# test -allCds
allCds: mkdirs
	mrnaToGene -genePredExt -allCds input/base.psl output/$@.gp
	genePredCheck output/$@.gp
	diff -u expected/$@.gp output/$@.gp

# test if psl header can be handled
header: mkdirs
	pslCat input/base.psl >output/basehdr.psl
	mrnaToGene -cdsFile=input/base.cds output/basehdr.psl output/$@.gp
	genePredCheck output/$@.gp
	diff -u expected/base.gp output/$@.gp

clean::
	rm -rf output

mkdirs:
	@${MKDIR} output
