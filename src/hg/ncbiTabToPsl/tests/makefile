include ../../../inc/common.mk

all:

test: refseqTest


# test with some refseqs
refseqTest: mkdirs
	../ncbiTabToPsl -qSizesFile=input/refseq.qsizes -tSizesFile=input/refseq.tsizes input/refseq.out output/refseqTest.psl
	pslCheck output/refseqTest.psl
	diff -u expected/refseqTest.psl output/refseqTest.psl

clean::
	rm -rf output

mkdirs:
	@mkdir -p output
