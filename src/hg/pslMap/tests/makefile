include ../../../inc/common.mk

all:

test: kgProt gapBoth mrnaMrnaMap mrnaMrnaXMap  mrnaMapOver


# basic tests of protein -> mrna -> genome mapping
kgProt: mkdirs
	blastToPsl input/kgProtMRna.blast output/kgProtMRna.psl
	../pslMap output/kgProtMRna.psl input/kgMRna.psl output/$@.psl
	pslCheck output/$@.psl
	diff -u expected/$@.psl output/$@.psl

# this generated a gap in both query and target
gapBoth: mkdirs
	blastToPsl input/gapBothProtMRna.blast output/gapBothProtMRna.psl
	../pslMap output/gapBothProtMRna.psl input/gapBothMRna.psl output/$@.psl
	pslCheck output/$@.psl
	diff -u expected/$@.psl output/$@.psl

# mrna->mrna->genome
mrnaMrnaMap: mkdirs
	../pslMap input/mrnaRefSeq.psl input/refSeqGen.psl output/$@.psl
	pslCheck output/$@.psl
	diff -u expected/$@.psl output/$@.psl

# mrna->mrna->genome, using translated mrna/mrna alignments
mrnaMrnaXMap: mkdirs
	../pslMap input/mrnaRefSeqX.psl input/refSeqGen.psl output/$@.psl
	pslCheck output/$@.psl
	diff -u expected/$@.psl output/$@.psl

# mrna->mm6->hg17, using untranslated mrna->mm6.  At one point this
# got the strand wrong.
mrnaMapOver: mkdirs
	../pslMap input/mrnaToMm6.psl input/mm6ToHg17.psl output/$@.psl
	pslCheck output/$@.psl
	diff -u expected/$@.psl output/$@.psl

clean::
	rm -rf output

mkdirs:
	@${MKDIR} output
