kentSrc = ../../../..
include ../../../../inc/common.mk

mafSplit=${DESTBINDIR}/mafSplit

all: test1 test2 test3

test1:
	${mafSplit} test1.bed test1 test1.maf
	cmp test1chrII.00.maf test1.correct.0.maf && \
	    cmp test1chrII.01.maf test1.correct.1.maf

test2:
	${mafSplit} test2.bed test2 test2.maf
	cmp test2chrII.00.maf test2.correct.0.maf && \
	    cmp test2chrII.01.maf test2.correct.1.maf

test3:
	${mafSplit} -byTarget -useFullSequenceName /dev/null test3 test3.maf
	cmp test3chrI.maf test3chrI.correct.maf && \
	    cmp test3chrII.maf test3chrII.correct.maf && \
	    cmp test3chrIII.maf test3chrIII.correct.maf

clean::
	rm -f test1chrII.01.maf test1chrII.00.maf
	rm -f test2chrII.00.maf test2chrII.01.maf
	rm -f test3chrI.maf test3chrIII.maf test3chrII.maf
