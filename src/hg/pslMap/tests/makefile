
all:

test: kgProt gapBoth mrnaMrnaMap mrnaXMrnaMap


# basic tests of protein -> mrna -> genome mapping
kgProt: mkdirs
	blastToPsl input/kgProtMRna.blast output/kgProtMRna.psl
	../pslMap output/kgProtMRna.psl input/kgMRna.psl output/kgProt.psl
	pslCheck output/kgProt.psl
	diff -u expected/kgProt.psl output/kgProt.psl

# this generated a gap in both query and target
gapBoth: mkdirs
	blastToPsl input/gapBothProtMRna.blast output/gapBothProtMRna.psl
	../pslMap output/gapBothProtMRna.psl input/gapBothMRna.psl output/gapBoth.psl
	pslCheck output/gapBoth.psl
	diff -u expected/gapBoth.psl output/gapBoth.psl

# mrna->mrna->genome
mrnaMrnaMap: mkdirs
	../pslMap input/mrnaRefSeq.psl input/refSeqGen.psl output/mrnaMrnaMap.psl
	pslCheck output/mrnaMrnaMap.psl
	diff -u expected/mrnaMrnaMap.psl output/mrnaMrnaMap.psl

# mrna->mrna->genome, using translated mrna/mrna alignments
mrnaXMrnaMap: mkdirs
	../pslMap input/mrnaRefSeqX.psl input/refSeqGen.psl output/mrnaMrnaXMap.psl
	pslCheck output/mrnaMrnaXMap.psl
	diff -u expected/mrnaMrnaXMap.psl output/mrnaMrnaXMap.psl

clean::
	rm -rf output

mkdirs:
	@mkdir -p output
