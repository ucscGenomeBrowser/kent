kentSrc = ../../..
include ../../../inc/common.mk

genePredToMafFrames = genePredToMafFrames
DIFF = diff -u

test:   oneExon splitExon revExon negNeg finishIsSplit \
	queryDel multiMap multiMap2 multiMap3 multiMapMondo \
	manyExonPos unalignedExonStart lostBlock splitNegStart \
	dupFrames revStrand twoOrg

# FIXME: corner case needs debugged: gapOne

oneExon: mkout
	${genePredToMafFrames} canHg10 input/BC101503.maf output/$@.mafFrames hg17 input/BC101503.gp
	${DIFF} expected/$@.mafFrames output/$@.mafFrames

# exon over two MAF blocks
splitExon: mkout
	${genePredToMafFrames} canHg10 input/NM_003906.maf output/$@.mafFrames hg17 input/NM_003906.gp
	${DIFF} expected/$@.mafFrames output/$@.mafFrames

# reversed strand, with only discontiguous exons aligned.
revExon: mkout
	${genePredToMafFrames} canHg10 input/revExon.maf output/$@.mafFrames hg17 input/revExon.gp
	${DIFF} expected/$@.mafFrames output/$@.mafFrames

# negative strand gene and query
negNeg: mkout
	${genePredToMafFrames} canHg10 input/NM_020727.maf output/$@.mafFrames hg17 input/NM_020727.gp
	${DIFF} expected/$@.mafFrames output/$@.mafFrames

# finishMafFrames bug detected in isSplitCodoni
finishIsSplit: mkout
	${genePredToMafFrames} canHg10 input/NM_001214.maf output/$@.mafFrames hg17 input/NM_001214.gp
	${DIFF} expected/$@.mafFrames output/$@.mafFrames

# 3-base deletion in query or insert in target 
queryDel: mkout
	${genePredToMafFrames} canHg10 input/NM_016427.maf output/$@.mafFrames hg17 input/NM_016427.gp
	${DIFF} expected/$@.mafFrames output/$@.mafFrames

# gene that maps to multiple locations
multiMap: mkout
	${genePredToMafFrames} canHg10 input/NM_207435.maf output/$@.mafFrames hg17 input/NM_207435.gp
	${DIFF} expected/$@.mafFrames output/$@.mafFrames

# another gene that maps to multiple locations
multiMap2: mkout
	${genePredToMafFrames} canHg10 input/NM_001452.maf output/$@.mafFrames hg17 input/NM_001452.gp
	${DIFF} expected/$@.mafFrames output/$@.mafFrames

# another multiMap trouble maker
multiMap3: mkout
	${genePredToMafFrames} mm7 input/multMap3.maf output/$@.mafFrames panTro1 input/multMap3.gp
	${DIFF} expected/$@.mafFrames output/$@.mafFrames

# multiple mappings, this one is ugly.
multiMapMondo: mkout
	${genePredToMafFrames} canHg10 input/NM_020185.maf output/$@.mafFrames hg17 input/NM_020185.gp
	${DIFF} expected/$@.mafFrames output/$@.mafFrames

# positive strand, multiple exons
manyExonPos: mkout
	${genePredToMafFrames} canHg10 input/NM_005682.maf output/$@.mafFrames hg17 input/NM_005682.gp
	${DIFF} expected/$@.mafFrames output/$@.mafFrames

unalignedExonStart: mkout
	${genePredToMafFrames} canHg10 input/NM_013321.maf output/$@.mafFrames hg17 input/NM_013321.gp
	${DIFF} expected/$@.mafFrames output/$@.mafFrames

# part of exon lost in mm6 but not human
lostBlock: mkout
	${genePredToMafFrames} canHg10 input/NM_145830.maf output/$@.mafFrames mm6 input/NM_145830.gp
	${DIFF} expected/$@.mafFrames output/$@.mafFrames

# start codon on neg strand gene not spliced, but split between two maf blocks
splitNegStart: mkout
	${genePredToMafFrames} mm7 input/NM_027863.maf output/$@.mafFrames mm7 input/NM_027863.gp
	${DIFF} expected/$@.mafFrames output/$@.mafFrames

# case that generated duplicate mafFrames rows
dupFrames: mkout
	${genePredToMafFrames} mm7 input/dupFrames.maf output/$@.mafFrames panTro1 input/dupFrames.gp
	${DIFF} expected/$@.mafFrames output/$@.mafFrames

# test using two components that are both reverse strand
revStrand: mkout
	${genePredToMafFrames} rn4 input/revStrand.maf output/$@.mafFrames danRer3 input/revStrand.gp
	${DIFF} expected/$@.mafFrames output/$@.mafFrames

# two organisms
twoOrg: mkout
	${genePredToMafFrames} canHg10 input/NM_145830.maf output/$@.mafFrames mm6 input/NM_145830.gp hg17 input/NM_006709.gp
	${DIFF} expected/$@.mafFrames output/$@.mafFrames

# regression: ended up with a one based gap in frames
gapOne: mkout
	${genePredToMafFrames} hg18 input/uc003izz.maf output/$@.mafFrames hg18 input/uc003izz.gp
	${DIFF} expected/$@.mafFrames output/$@.mafFrames

mkout:
	@${MKDIR} output

clean:
	rm -rf output
