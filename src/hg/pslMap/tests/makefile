
all:

test: kgProt gapBoth


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

clean::
	rm -rf output

mkdirs:
	@mkdir -p output
