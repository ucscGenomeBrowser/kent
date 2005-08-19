include ../../../inc/common.mk

all:

test:	protMrnaTest protDnaTest mrnaDnaTest mrnaDnaxTest \
	blockBugTest identityBugTest protMrnaEval1Test protMrnaEval2Test \
	longDbNameTest


protMrnaTest: mkdirs
	../blastToPsl -scores=output/$@.bscores input/kgProtMRna1.blast output/$@.psl
	pslCheck output/$@.psl
	diff -u expected/$@.psl output/$@.psl
	diff -u expected/$@.bscores output/$@.bscores

protDnaTest: mkdirs
	../blastToPsl -scores=output/$@.bscores input/kgProtDna1.blast output/$@.psl
	pslCheck output/$@.psl
	diff -u expected/$@.psl output/$@.psl
	diff -u expected/$@.bscores output/$@.bscores

mrnaDnaTest: mkdirs
	../blastToPsl input/mrnaDna1.blast output/$@.psl
	pslCheck output/$@.psl
	diff -u expected/$@.psl output/$@.psl

mrnaDnaxTest: mkdirs
	../blastToPsl -pslx input/mrnaDna1.blast output/$@.psl
	pslCheck output/$@.psl
	diff -u expected/$@.psl output/$@.psl

# test case that cause huge target block start.
blockBugTest: mkdirs
	../blastToPsl input/blockBug.blast output/$@.psl
	pslCheck output/$@.psl
	diff -u expected/$@.psl output/$@.psl

# handle id
identityBugTest: mkdirs
	../blastToPsl input/identityBug.blast output/$@.psl
	pslCheck output/$@.psl
	diff -u expected/$@.psl output/$@.psl

# test E-value threshold filter
protMrnaEval1Test: mkdirs
	../blastToPsl -eVal=1 -scores=output/$@.bscores input/kgProtMRna1.blast output/$@.psl
	pslCheck output/$@.psl
	diff -u expected/$@.psl output/$@.psl
	diff -u expected/$@.bscores output/$@.bscores

protMrnaEval2Test: mkdirs
	../blastToPsl -eVal=1e-10 -scores=output/$@.bscores input/kgProtMRna1.blast output/$@.psl
	pslCheck output/$@.psl
	diff -u expected/$@.psl output/$@.psl
	diff -u expected/$@.bscores output/$@.bscores

longDbNameTest: mkdirs
	../blastToPsl input/longDbName.blast output/$@.psl
	pslCheck output/$@.psl
	diff -u expected/$@.psl output/$@.psl


# this test is currently disabled, mega-blast is painful because
# Database: comes before Query= and there are multiple queries for each
# Database:.
$@: mkdirs
	awk -f ../mega.awk input/970.txt > output/$@.blast
	../blastToPsl output/$@.blast output/$@.psl
	pslCheck output/$@.psl
	diff -u expected/$@.psl output/$@.psl


clean::
	rm -rf output

mkdirs:
	@${MKDIR} output
