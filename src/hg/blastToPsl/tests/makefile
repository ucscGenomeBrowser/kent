include ../../../inc/common.mk

all:

test: protMrnaTest protDnaTest mrnaDnaTest blockBugTest identityBugTest protMrnaEval1Test protMrnaEval2Test


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

# test case that cause huge target block start.
blockBugTest: mkdirs
	../blastToPsl input/blockBug.blast output/blockBugTest.psl
	pslCheck output/blockBugTest.psl
	diff -u expected/blockBugTest.psl output/blockBugTest.psl

# handle id
identityBugTest: mkdirs
	../blastToPsl input/identityBug.blast output/identityBugTest.psl
	pslCheck output/identityBugTest.psl
	diff -u expected/identityBugTest.psl output/identityBugTest.psl

# test E-value threshold filter
protMrnaEval1Test: mkdirs
	../blastToPsl -eVal=1 -scores=output/protMrnaEval1Test.bscores input/kgProtMRna1.blast output/protMrnaEval1Test.psl
	pslCheck output/protMrnaEval1Test.psl
	diff -u expected/protMrnaEval1Test.psl output/protMrnaEval1Test.psl
	diff -u expected/protMrnaEval1Test.bscores output/protMrnaEval1Test.bscores

protMrnaEval2Test: mkdirs
	../blastToPsl -eVal=1e-10 -scores=output/protMrnaEval2Test.bscores input/kgProtMRna1.blast output/protMrnaEval2Test.psl
	pslCheck output/protMrnaEval2Test.psl
	diff -u expected/protMrnaEval2Test.psl output/protMrnaEval2Test.psl
	diff -u expected/protMrnaEval2Test.bscores output/protMrnaEval2Test.bscores


	diff -u expected/protMrnaTest.bscores output/protMrnaTest.bscores

# this test is currently disabled, mega-blast is painful because
# Database: comes before Query= and there are multiple queries for each
# Database:.
megaBlastTest: mkdirs
	awk -f ../mega.awk input/970.txt > output/megaBlastTest.blast
	../blastToPsl output/megaBlastTest.blast output/megaBlastTest.psl
	pslCheck output/megaBlastTest.psl
	diff -u expected/megaBlastTest.psl output/megaBlastTest.psl


clean::
	rm -rf output

mkdirs:
	@${MKDIR} output
