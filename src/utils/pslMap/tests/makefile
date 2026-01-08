kentSrc = ../../..
include ${kentSrc}/inc/common.mk

#pslMap = valgrind --tool=memcheck --num-callers=25 pslMap
pslMap = ${DESTBINDIR}/pslMap
pslSwap = ${DESTBINDIR}/pslSwap
blastToPsl = ${DESTBINDIR}/blastToPsl
pslCheck = ${DESTBINDIR}/pslCheck
chainSwap = ${DESTBINDIR}/chainSwap

all:

test:	gapBoth mrnaMrnaMap mrnaMrnaXMap mrnaMapOver mrnaMapOverSwap \
	mrnaMapOverChain mrnaMapOverChainSwap testMapFileWithInQName \
	kgProt negQChain testProtPort_ProtNa testProtPort_ProtProt \
	testProtNa_NaNa testProtProt_NaNa \
	testTypeErr testProtTrans \
	mrnaMapOverHttp mrnaMapOverChainHttp

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
	${pslMap} -mapFileWithInQName -mapInfo=output/$@.mapinfo input/gencode.src.psl input/gencode.blocks.qName-psl output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl
	diff -u expected/$@.mapinfo output/$@.mapinfo

# swissprot canonical features that were mapped to isoform with protein
# isoforms aligned to gencode RNAs.  Mapping these resulted in overlapping
# blocks.  This tests block adjustments as well
#    prot_prot   prot_na     cds_na
testProtPort_ProtNa: mkdirs
	${pslMap} -inType=prot_prot -mapInfo=output/$@.mapinfo input/spAnnotIso.psl input/spGencode.psl output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl
	diff -u expected/$@.mapinfo output/$@.mapinfo

# Mapping protein alignments with protein alignments, just done with swapped PSL as map
# This mainly tests options, as there is no difference between this and NA mappings.  
#   prot_prot   prot_prot   prot_prot
testProtPort_ProtProt: mkdirs
	pslSwap input/spAnnotIso.psl output/$@.mapping.psl
	${pslMap} -inType=prot_prot -mapType=prot_prot input/spAnnotIso.psl output/$@.mapping.psl output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl

# prot_na     na_na       cds_na
testProtNa_NaNa: mkdirs
	${pslMap} -inType=prot_na -mapType=na_na input/spGencode.psl input/gencodeGenome.psl output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl

# prot_prot   na_na       cds_na
testProtProt_NaNa: mkdirs
	${pslMap} -inType=prot_prot -mapType=na_na input/spAnnotIso.psl expected/testProtNa_NaNa.psl output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl

# implied types will not work with prot_na map:
#    prot_prot   prot_na     cds_na
testTypeErr: mkdirs
	if ${pslMap} input/spAnnotIso.psl input/spGencode.psl output/$@.psl  2> output/$@.err ; then  echo "Error: should have failed" >&2; false ; else true ; fi
	diff expected/$@.err output/$@.err

# bug where swissprot feature to protein is mapped via protein to transcript, genereated
# an invalid PSL with an overlapping block
testProtTrans: mkdirs
	${pslMap} -inType=prot_prot -mapType=prot_na input/annotProt.psl input/protGencode.psl output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl

# remote access
mrnaToMm6_psl_https = https://raw.githubusercontent.com/ucscGenomeBrowser/kent/refs/heads/master/src/utils/pslMap/tests/input/mrnaToMm6.psl
mm6ToHg17_psl_https = https://raw.githubusercontent.com/ucscGenomeBrowser/kent/refs/heads/master/src/utils/pslMap/tests/input/mm6ToHg17.psl
mm6ToHg17_chain_https = https://raw.githubusercontent.com/ucscGenomeBrowser/kent/refs/heads/master/src/utils/pslMap/tests/input/mm6ToHg17.chain

mrnaMapOverHttp: mkdirs
	${pslMap} -mapInfo=output/$@.mapinfo ${mrnaToMm6_psl_https} ${mm6ToHg17_psl_https} output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	diff -u expected/mrnaMapOver.psl output/$@.psl
	diff -u expected/mrnaMapOver.mapinfo output/$@.mapinfo

mrnaMapOverChainHttp: mkdirs
	${pslMap} -chainMapFile -mappingPsls=output/$@.mapping.psl ${mrnaToMm6_psl_https} ${mm6ToHg17_chain_https} output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	${pslCheck} -verbose=0 output/$@.mapping.psl
	diff -u expected/mrnaMapOver.psl output/$@.psl
	diff -u expected/mrnaMapOverChain.mapping.psl output/$@.mapping.psl


clean::
	rm -rf output

mkdirs:
	@${MKDIR} output
