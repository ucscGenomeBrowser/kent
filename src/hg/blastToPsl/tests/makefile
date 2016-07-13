kentSrc = ../../..
include ../../../inc/common.mk

#blastToPsl = valgrind --tool=memcheck  blastToPsl
blastToPsl = ${DESTBINDIR}/blastToPsl

#blastXmlToPsl = valgrind --tool=memcheck  blastXmlToPsl
blastXmlToPsl =  ${DESTBINDIR}/blastXmlToPsl

pslCheck =  ${DESTBINDIR}/pslCheck

all:

test: testBlastToPsl testBlastXmlToPsl

###
# blastToPsl
###
testBlastToPsl:	protMrnaTest protDnaTest mrnaDnaTest mrnaDnaxTest \
	blockBugTest identityBugTest protMrnaEval1Test protMrnaEval2Test \
	longDbNameTest multiLineDescTest tblastxTest tblastnTest \
	blastpPsiTest psiNoHitsTest psiBadFormatTest blastpPsiWrapTest

protMrnaTest: mkdirs
	${blastToPsl} -scores=output/$@.bscores input/kgProtMRna1.blast output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl
	diff -u expected/$@.bscores output/$@.bscores

protDnaTest: mkdirs
	${blastToPsl} -scores=output/$@.bscores input/kgProtDna1.blast output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl
	diff -u expected/$@.bscores output/$@.bscores

mrnaDnaTest: mkdirs
	${blastToPsl} input/mrnaDna1.blast output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl

mrnaDnaxTest: mkdirs
	${blastToPsl} -pslx input/mrnaDna1.blast output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl

# test case that cause huge target block start.
blockBugTest: mkdirs
	${blastToPsl} input/blockBug.blast output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl

# handle id
identityBugTest: mkdirs
	${blastToPsl} input/identityBug.blast output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl

# test E-value threshold filter
protMrnaEval1Test: mkdirs
	${blastToPsl} -eVal=1 -scores=output/$@.bscores input/kgProtMRna1.blast output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl
	diff -u expected/$@.bscores output/$@.bscores

protMrnaEval2Test: mkdirs
	${blastToPsl} -eVal=1e-10 -scores=output/$@.bscores input/kgProtMRna1.blast output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl
	diff -u expected/$@.bscores output/$@.bscores

longDbNameTest: mkdirs
	${blastToPsl} input/longDbName.blast output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl

# multiline description broke parser
multiLineDescTest: mkdirs
	${blastToPsl} input/multiLineDesc.blast output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl

# tblastx
tblastxTest: mkdirs
	${blastToPsl} -scores=output/$@.bscores input/tblastx.blast output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl
	diff -u expected/$@.bscores output/$@.bscores

# tblastn
tblastnTest: mkdirs
	${blastToPsl} -scores=output/$@.bscores input/tblastn.blast output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl
	diff -u expected/$@.bscores output/$@.bscores


# this test is currently disabled.  This file has Database: before Query= and
# there are multiple queries for each Database:
tblastnDbFirstTest: mkdirs
	${blastToPsl} input/tblastnDbFirst.blast output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl


# this test is currently disabled, mega-blast is painful because
# Database: comes before Query= and there are multiple queries for each
# Database:.
megaBlastTest: mkdirs
	${blastToPsl} input/970.blast output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl

# PSI-BLASTP 
blastpPsiTest: mkdirs
	${blastToPsl} -scores=output/$@.bscores input/blastpPsi.blast output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl
	diff -u expected/$@.bscores output/$@.bscores

# PSI-BLASTP with Identities line wrapping
blastpPsiWrapTest: mkdirs
	${blastToPsl} -scores=output/$@.bscores input/blastpPsiWrap.blast output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
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
	${pslCheck} -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl
	diff -u expected/$@.bscores output/$@.bscores
	diff -u expected/$@.err output/$@.err

###
# blastXmlToPsl
###
testBlastXmlToPsl: mrna1Test prot1Test prot2Test prot1NucCoordTest sp1PsiBlastTest qtName1Test qtName2Test misclassedPsiBlastTest

mrna1Test: mkdirs
	${blastXmlToPsl} -scores=output/$@.bscores input/mrna1.blastn.xml output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl
	diff -u expected/$@.bscores output/$@.bscores

prot1Test: mkdirs
	${blastXmlToPsl} -scores=output/$@.bscores input/prot1.tblastn.xml output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl
	diff -u expected/$@.bscores output/$@.bscores

prot2Test: mkdirs
	${blastXmlToPsl} -scores=output/$@.bscores input/prot2.psiblast.xml output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl
	diff -u expected/$@.bscores output/$@.bscores

prot1NucCoordTest: mkdirs
	${blastXmlToPsl} -convertToNucCoords input/prot1.tblastn.xml output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl

sp1PsiBlastTest: mkdirs
	${blastXmlToPsl} -tName=Hit_accession input/sp1.psiBlast.xml output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl

qtName1Test: mkdirs
	${blastXmlToPsl} -qName=query-ID -tName=Hit_id input/sp1.psiBlast.xml output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl

qtName2Test: mkdirs
	${blastXmlToPsl} -qName=query-def0 -tName=Hit_def0 input/sp1.psiBlast.xml output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl

# psiblast file that claimes to be blastp
misclassedPsiBlastTest: mkdirs
	${blastXmlToPsl} -forcePsiBlast -tName=Hit_accession -scores=output/$@.bscores input/misclassed.psiBlast.xml output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl
	diff -u expected/$@.bscores output/$@.bscores

clean::
	rm -rf output

mkdirs:
	@${MKDIR} -p output
