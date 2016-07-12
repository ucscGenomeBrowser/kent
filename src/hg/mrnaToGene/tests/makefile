kentSrc = ../../..
include ../../../inc/common.mk

mrnaToGene = ${DESTBINDIR}/mrnaToGene
genePredCheck = ${DESTBINDIR}/genePredCheck
pslCat = ${DESTBINDIR}/pslCat

all:

test: base frame allCds noCds header noGapMerge gapMerge cds3GapMerge trans

# basic set of tests
base: mkdirs
	${mrnaToGene} -cdsFile=input/base.cds input/base.psl output/$@.gp
	${genePredCheck} -verbose=0 output/$@.gp
	diff -u expected/$@.gp output/$@.gp

# test with frame info
frame: mkdirs
	${mrnaToGene} -genePredExt -cdsFile=input/base.cds input/base.psl output/$@.gp
	${genePredCheck} -verbose=0 output/$@.gp
	diff -u expected/$@.gp output/$@.gp

# test -allCds
allCds: mkdirs
	${mrnaToGene} -allCds input/base.psl output/$@.gp
	${genePredCheck} -verbose=0 output/$@.gp
	diff -u expected/$@.gp output/$@.gp

# test -noCds
noCds: mkdirs
	${mrnaToGene} -genePredExt -noCds input/base.psl output/$@.gp
	${genePredCheck} -verbose=0 output/$@.gp
	diff -u expected/$@.gp output/$@.gp

# test if psl header can be handled
header: mkdirs
	${pslCat} input/base.psl >output/basehdr.psl
	${mrnaToGene} -cdsFile=input/base.cds output/basehdr.psl output/$@.gp
	${genePredCheck} -verbose=0 output/$@.gp
	diff -u expected/base.gp output/$@.gp

# don't do any gap merging
noGapMerge: mkdirs
	${mrnaToGene} -cdsMergeSize=-1 -cdsMergeMod3 -utrMergeSize=-1 -genePredExt -cdsFile=input/gaps.cds input/gaps.psl output/$@.gp
	${genePredCheck} -verbose=0 output/$@.gp
	diff -u expected/$@.gp output/$@.gp

# gap merging, no CDS mod 3, different CDS and UTR sizes
gapMerge: mkdirs
	${mrnaToGene} -cdsMergeSize=6 -utrMergeSize=10 -genePredExt -cdsFile=input/gaps.cds input/gaps.psl output/$@.gp
	${genePredCheck} -verbose=0 output/$@.gp
	diff -u expected/$@.gp output/$@.gp


# test merging CDS mod 3 gaps, and UTR
cds3GapMerge: mkdirs
	${mrnaToGene} -cdsMergeSize=6 -cdsMergeMod3 -utrMergeSize=10 -genePredExt -cdsFile=input/gaps.cds input/gaps.psl output/$@.gp
	${genePredCheck} -verbose=0 output/$@.gp
	diff -u expected/$@.gp output/$@.gp

# translated psls; -+ and -- cases are faked
trans:
	${mrnaToGene} -cdsMergeSize=6 -cdsMergeMod3 -utrMergeSize=10 -cdsFile=input/trans.cds input/trans.psl output/$@.gp
	${genePredCheck} -verbose=0 output/$@.gp
	diff -u expected/$@.gp output/$@.gp

clean::
	rm -rf output

mkdirs:
	@${MKDIR} output
