kentSrc = ../../../..
include ../../../../inc/common.mk

mafSplit=${DESTBINDIR}/mafSplit

test1:
	${mafSplit} test1.bed test1 test1.maf
	cmp test1chrII.00.maf test1.correct.0.maf && \
	    cmp test1chrII.01.maf test1.correct.1.maf 

clean::
	echo nothing to clean in hg/ratStuff/mafSplit/test/
