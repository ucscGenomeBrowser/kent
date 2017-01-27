kentSrc = ../../../..
include ${kentSrc}/inc/common.mk

#pslMap = valgrind --tool=memcheck --num-callers=25 pslMap
pslMap = ${DESTBINDIR}/pslMap
pslSwap = ${DESTBINDIR}/pslSwap
blastToPsl = ${DESTBINDIR}/blastToPsl
pslCheck = ${DESTBINDIR}/pslCheck
chainSwap = ${DESTBINDIR}/chainSwap

all:

test:	gapBoth mrnaMrnaMap mrnaMrnaXMap mrnaMapOver mrnaMapOverSwap \
	mrnaMapOverChain mrnaMapOverChainSwap testMapFileWithInQName

# things with large files that we want to remove their big history from cvs
# but soon will replace with short versions and no big history.
# temporarily moved to /hive/users/galt/largeCvsFiles/kent/...
# input/kgMRna.psl
# input/negQ.chain
testnot: kgProt negQChain

# basic tests of protein -> mrna -> genome mapping
kgProt: mkdirs
	${blastToPsl} input/kgProtMRna.blast output/kgProtMRna.psl
	${pslMap} output/kgProtMRna.psl input/kgMRna.psl output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl

# this generated a gap in both query and target
gapBoth: mkdirs
	${blastToPsl} input/gapBothProtMRna.blast output/gapBothProtMRna.psl
	${pslMap} output/gapBothProtMRna.psl input/gapBothMRna.psl output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl

# mrna->mrna->genome
mrnaMrnaMap: mkdirs
	${pslMap} input/mrnaRefSeq.psl input/refSeqGen.psl output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl

# mrna->mrna->genome, using translated mrna/mrna alignments
mrnaMrnaXMap: mkdirs
	${pslMap} input/mrnaRefSeqX.psl input/refSeqGen.psl output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl

# mrna->mm6->hg17, using untranslated mrna->mm6.  At one point this
# got the strand wrong.
# also check mapInfo on unmapped entries
mrnaMapOver: mkdirs
	${pslMap} -mapInfo=output/$@.mapinfo input/mrnaToMm6.psl input/mm6ToHg17.psl output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl
	diff -u expected/$@.mapinfo output/$@.mapinfo

# swapped mapping psl, same result as mrnaMapOver
mrnaMapOverSwap: mkdirs
	${pslSwap} -noRc input/mm6ToHg17.psl output/hg17ToMm6.psl
	${pslMap} -swapMap -mappingPsls=output/$@.mapping.psl -mapInfo=output/$@.mapinfo input/mrnaToMm6.psl output/hg17ToMm6.psl output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	${pslCheck} -verbose=0 output/$@.mapping.psl
	diff -u expected/mrnaMapOver.psl output/$@.psl
	diff -u expected/$@.mapinfo output/$@.mapinfo
	diff -u expected/$@.mapping.psl output/$@.mapping.psl

# use chain, same results as mrnaMapOver
mrnaMapOverChain: mkdirs
	${pslMap} -chainMapFile -mappingPsls=output/$@.mapping.psl input/mrnaToMm6.psl input/mm6ToHg17.chain output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	${pslCheck} -verbose=0 output/$@.mapping.psl
	diff -u expected/mrnaMapOver.psl output/$@.psl
	diff -u expected/$@.mapping.psl output/$@.mapping.psl

# use chain swapped, same results as mrnaMapOver
mrnaMapOverChainSwap: mkdirs
	${chainSwap} input/mm6ToHg17.chain output/hg17ToMm6.chain 
	${pslMap} -chainMapFile -swapMap input/mrnaToMm6.psl output/hg17ToMm6.chain output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	diff -u expected/mrnaMapOver.psl output/$@.psl

# bug created with binKeeper optimization where negative strand query chains
# would get lost
negQChain: mkdirs
	${pslMap} -chainMapFile input/negQ.refSeq.psl input/negQ.chain output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl

# retro/parent alignment through mRNA, with -simplifyMappingIds
retroParent: mkdirs
	${pslMap} -simplifyMappingIds -swapIn -mapInfo=output/$@.mapinfo input/retro.psl input/parent.psl output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl
	diff -u expected/$@.mapinfo output/$@.mapinfo

# -mapFileWithInQName
testMapFileWithInQName: mkdirs
	${pslMap} -mapFileWithInQName input/gencode.src.psl input/gencode.blocks.qName-psl output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl

clean::
	rm -rf output

mkdirs:
	@${MKDIR} output
