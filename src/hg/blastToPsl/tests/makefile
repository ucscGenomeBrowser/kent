include ../../../inc/common.mk

#blastToPsl = valgrind --tool=memcheck ../blastToPsl
blastToPsl = ../blastToPsl

all:

test:	protMrnaTest protDnaTest mrnaDnaTest mrnaDnaxTest \
	blockBugTest identityBugTest protMrnaEval1Test protMrnaEval2Test \
	longDbNameTest multiLineDescTest tblastxTest tblastnTest \
	blastpPsiTest psiNoHitsTest psiBadFormatTest blastpPsiWrapTest \

# blast+ not implemented yet
#	blastn+Test

#tblastnDbFirstTest megaBlastTest

protMrnaTest: mkdirs
	${blastToPsl} -scores=output/$@.bscores input/kgProtMRna1.blast output/$@.psl
	pslCheck -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl
	diff -u expected/$@.bscores output/$@.bscores

protDnaTest: mkdirs
	${blastToPsl} -scores=output/$@.bscores input/kgProtDna1.blast output/$@.psl
	pslCheck -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl
	diff -u expected/$@.bscores output/$@.bscores

mrnaDnaTest: mkdirs
	${blastToPsl} input/mrnaDna1.blast output/$@.psl
	pslCheck -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl

mrnaDnaxTest: mkdirs
	${blastToPsl} -pslx input/mrnaDna1.blast output/$@.psl
	pslCheck -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl

# test case that cause huge target block start.
blockBugTest: mkdirs
	${blastToPsl} input/blockBug.blast output/$@.psl
	pslCheck -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl

# handle id
identityBugTest: mkdirs
	${blastToPsl} input/identityBug.blast output/$@.psl
	pslCheck -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl

# test E-value threshold filter
protMrnaEval1Test: mkdirs
	${blastToPsl} -eVal=1 -scores=output/$@.bscores input/kgProtMRna1.blast output/$@.psl
	pslCheck -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl
	diff -u expected/$@.bscores output/$@.bscores

protMrnaEval2Test: mkdirs
	${blastToPsl} -eVal=1e-10 -scores=output/$@.bscores input/kgProtMRna1.blast output/$@.psl
	pslCheck -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl
	diff -u expected/$@.bscores output/$@.bscores

longDbNameTest: mkdirs
	${blastToPsl} input/longDbName.blast output/$@.psl
	pslCheck -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl

# multiline description broke parser
multiLineDescTest: mkdirs
	${blastToPsl} input/multiLineDesc.blast output/$@.psl
	pslCheck -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl

# tblastx
tblastxTest: mkdirs
	${blastToPsl} -scores=output/$@.bscores input/tblastx.blast output/$@.psl
	pslCheck -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl
	diff -u expected/$@.bscores output/$@.bscores

# tblastn
tblastnTest: mkdirs
	${blastToPsl} -scores=output/$@.bscores input/tblastn.blast output/$@.psl
	pslCheck -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl
	diff -u expected/$@.bscores output/$@.bscores


# this test is currently disabled.  This file has Database: before Query= and
# there are multiple queries for each Database:
tblastnDbFirstTest: mkdirs
	${blastToPsl} input/tblastnDbFirst.blast output/$@.psl
	pslCheck -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl


# this test is currently disabled, mega-blast is painful because
# Database: comes before Query= and there are multiple queries for each
# Database:.
megaBlastTest: mkdirs
	${blastToPsl} input/970.blast output/$@.psl
	pslCheck -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl

# PSI-BLASTP 
blastpPsiTest: mkdirs
	${blastToPsl} -scores=output/$@.bscores input/blastpPsi.blast output/$@.psl
	pslCheck -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl
	diff -u expected/$@.bscores output/$@.bscores

# PSI-BLASTP with Identities line wrapping
blastpPsiWrapTest: mkdirs
	${blastToPsl} -scores=output/$@.bscores input/blastpPsiWrap.blast output/$@.psl
	pslCheck -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl
	diff -u expected/$@.bscores output/$@.bscores

# PSI-BLASTP with no hits
psiNoHitsTest: mkdirs
	${blastToPsl} -scores=output/$@.bscores input/psiNoHits.blast output/$@.psl
	diff -u expected/$@.psl output/$@.psl
	diff -u expected/$@.bscores output/$@.bscores

# PSI-BLASTP produces bogus blocks where query starts at 0 and there is not
# ending number on the line. Sbjct may be empty or have sequence.  These 
# are ignored
psiBadFormatTest: mkdirs
	${blastToPsl} -scores=output/$@.bscores input/psiBadFormat.blast output/$@.psl >output/$@.err 2>&1
	pslCheck -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl
	diff -u expected/$@.bscores output/$@.bscores
	diff -u expected/$@.err output/$@.err

# blast+ changes output format
blastn+Test: mkdirs
	${blastToPsl} -scores=output/$@.bscores input/blastn+.blast output/$@.psl >output/$@.err 2>&1
	pslCheck -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl
	diff -u expected/$@.bscores output/$@.bscores


clean::
	rm -rf output

mkdirs:
	@${MKDIR} output
