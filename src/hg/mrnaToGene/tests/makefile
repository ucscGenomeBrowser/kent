include ../../../inc/common.mk

all:

test: base frame

# basic set of tests
base: mkdirs
	mrnaToGene -cdsFile=input/base.cds input/base.psl output/base.gp
	diff -u expected/base.gp output/base.gp

# test with frame info
frame: mkdirs
	mrnaToGene -genePredExt -cdsFile=input/base.cds input/base.psl output/frame.gp
	diff -u expected/frame.gp output/frame.gp

clean::
	rm -rf output

mkdirs:
	@${MKDIR} output
