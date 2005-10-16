include ../../../inc/common.mk

DIFF = diff -u

test:  oneExon splitExon revExon negNeg finishIsSplit queryDel multiMap

oneExon: mkout
	genePredToMafFrames hg17 canHg10 input/BC101503.gp output/$@.mafFrames input/BC101503.maf
	${DIFF} expected/$@.mafFrames output/$@.mafFrames

# exon over two MAF blocks
splitExon:
	genePredToMafFrames hg17 canHg10 input/NM_003906.gp output/$@.mafFrames input/NM_003906.maf
	${DIFF} expected/$@.mafFrames output/$@.mafFrames

# reversed strand, with only discontiguous exons aligned.
revExon:
	genePredToMafFrames hg17 canHg10 input/revExon.gp output/$@.mafFrames input/revExon.maf
	${DIFF} expected/$@.mafFrames output/$@.mafFrames

# negative strand gene and query
negNeg:
	genePredToMafFrames hg17 canHg10 input/NM_020727.gp output/$@.mafFrames input/NM_020727.maf
	${DIFF} expected/$@.mafFrames output/$@.mafFrames

# finishMafFrames bug detected in isSplitCodoni
finishIsSplit:
	genePredToMafFrames hg17 canHg10 input/NM_001214.gp output/$@.mafFrames input/NM_001214.maf
	${DIFF} expected/$@.mafFrames output/$@.mafFrames

# 3-base deletion in query or insert in target 
queryDel:
	genePredToMafFrames hg17 canHg10 input/NM_016427.gp output/$@.mafFrames input/NM_016427.maf
	${DIFF} expected/$@.mafFrames output/$@.mafFrames

# gene that maps to multiple locations
multiMap:
	genePredToMafFrames hg17 canHg10 input/NM_207435.gp output/$@.mafFrames input/NM_207435.maf
	${DIFF} expected/$@.mafFrames output/$@.mafFrames

mkout:
	@${MKDIR} output

clean:
	rm -rf output
