include ../../../inc/common.mk

all:

test: protMrnaTest protDnaTest mrnaDnaTest


protMrnaTest: mkdirs
	../blastToPsl -scores=output/protMrnaTest.bscores input/kgProtMRna1.blast output/protMrnaTest.psl
	pslCheck output/protMrnaTest.psl
	diff -u expected/protMrnaTest.psl output/protMrnaTest.psl
	diff -u expected/protMrnaTest.bscores output/protMrnaTest.bscores

protDnaTest: mkdirs
	../blastToPsl -scores=output/protDnaTest.bscores input/kgProtDna1.blast output/protDnaTest.psl
	pslCheck output/protDnaTest.psl
	diff -u expected/protDnaTest.psl output/protDnaTest.psl
	diff -u expected/protDnaTest.bscores output/protDnaTest.bscores

mrnaDnaTest: mkdirs
	../blastToPsl input/mrnaDna1.blast output/mrnaDnaTest.psl
	pslCheck output/mrnaDnaTest.psl
	diff -u expected/mrnaDnaTest.psl output/mrnaDnaTest.psl

clean::
	rm -rf output

mkdirs:
	@mkdir -p output
