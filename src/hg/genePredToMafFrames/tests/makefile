include ../../../inc/common.mk

genePredToMafFrames = genePredToMafFrames
DIFF = diff -u

test:   oneExon splitExon revExon negNeg finishIsSplit \
	queryDel multiMap multiMap2 multiMap3 multiMapMondo \
	manyExonPos unalignedExonStart lostBlock splitNegStart \
	dupFrames revStrand oneOrg oneOrgComma twoOrg

oneExon: mkout
	${genePredToMafFrames} hg17 canHg10 input/BC101503.gp output/$@.mafFrames input/BC101503.maf
	${DIFF} expected/$@.mafFrames output/$@.mafFrames

# exon over two MAF blocks
splitExon: mkout
	${genePredToMafFrames} hg17 canHg10 input/NM_003906.gp output/$@.mafFrames input/NM_003906.maf
	${DIFF} expected/$@.mafFrames output/$@.mafFrames

# reversed strand, with only discontiguous exons aligned.
revExon: mkout
	${genePredToMafFrames} hg17 canHg10 input/revExon.gp output/$@.mafFrames input/revExon.maf
	${DIFF} expected/$@.mafFrames output/$@.mafFrames

# negative strand gene and query
negNeg: mkout
	${genePredToMafFrames} hg17 canHg10 input/NM_020727.gp output/$@.mafFrames input/NM_020727.maf
	${DIFF} expected/$@.mafFrames output/$@.mafFrames

# finishMafFrames bug detected in isSplitCodoni
finishIsSplit: mkout
	${genePredToMafFrames} hg17 canHg10 input/NM_001214.gp output/$@.mafFrames input/NM_001214.maf
	${DIFF} expected/$@.mafFrames output/$@.mafFrames

# 3-base deletion in query or insert in target 
queryDel: mkout
	${genePredToMafFrames} hg17 canHg10 input/NM_016427.gp output/$@.mafFrames input/NM_016427.maf
	${DIFF} expected/$@.mafFrames output/$@.mafFrames

# gene that maps to multiple locations
multiMap: mkout
	${genePredToMafFrames} hg17 canHg10 input/NM_207435.gp output/$@.mafFrames input/NM_207435.maf
	${DIFF} expected/$@.mafFrames output/$@.mafFrames

# another gene that maps to multiple locations
multiMap2: mkout
	${genePredToMafFrames} hg17 canHg10 input/NM_001452.gp output/$@.mafFrames input/NM_001452.maf
	${DIFF} expected/$@.mafFrames output/$@.mafFrames

# another multiMap trouble maker
multiMap3: mkout
	${genePredToMafFrames} panTro1 mm7 input/multMap3.gp output/$@.mafFrames input/multMap3.maf
	${DIFF} expected/$@.mafFrames output/$@.mafFrames

# multiple mappings, this one is ugly.
multiMapMondo: mkout
	${genePredToMafFrames} hg17 canHg10 input/NM_020185.gp output/$@.mafFrames input/NM_020185.maf
	${DIFF} expected/$@.mafFrames output/$@.mafFrames

# positive strand, multiple exons
manyExonPos: mkout
	${genePredToMafFrames} hg17 canHg10 input/NM_005682.gp output/$@.mafFrames input/NM_005682.maf
	${DIFF} expected/$@.mafFrames output/$@.mafFrames

unalignedExonStart: mkout
	${genePredToMafFrames} hg17 canHg10 input/NM_013321.gp output/$@.mafFrames input/NM_013321.maf
	${DIFF} expected/$@.mafFrames output/$@.mafFrames

# part of exon lost in mm6 but not human
lostBlock: mkout
	${genePredToMafFrames} mm6 canHg10 input/NM_145830.gp output/$@.mafFrames input/NM_145830.maf
	${DIFF} expected/$@.mafFrames output/$@.mafFrames

# start codon on neg strand gene not spliced, but split between two maf blocks
splitNegStart: mkout
	${genePredToMafFrames} mm7 mm7 input/NM_027863.gp output/$@.mafFrames input/NM_027863.maf
	${DIFF} expected/$@.mafFrames output/$@.mafFrames

# case that generated duplicate mafFrames rows
dupFrames: mkout
	${genePredToMafFrames} panTro1 mm7 input/dupFrames.gp output/$@.mafFrames input/dupFrames.maf
	${DIFF} expected/$@.mafFrames output/$@.mafFrames

# test using two components that are both reverse strand
revStrand: mkout
	${genePredToMafFrames} danRer3 rn4 input/revStrand.gp output/$@.mafFrames input/revStrand.maf
	${DIFF} expected/$@.mafFrames output/$@.mafFrames

# -org argument with one database, should be same as oneExon test
oneOrg: mkout
	${genePredToMafFrames} -org="hg17 input/BC101503.gp" canHg10 output/$@.mafFrames input/BC101503.maf
	${DIFF} expected/oneExon.mafFrames output/$@.mafFrames

# -org argument, comma separated with one database , should be same as oneExon test
oneOrgComma: mkout
	${genePredToMafFrames} -org=hg17,input/BC101503.gp canHg10 output/$@.mafFrames input/BC101503.maf
	${DIFF} expected/oneExon.mafFrames output/$@.mafFrames

# -org with two organisms
twoOrg: mkout
	${genePredToMafFrames} -org=mm6,input/NM_145830.gp -org='hg17  input/NM_006709.gp' canHg10 output/$@.mafFrames input/NM_145830.maf
	${DIFF} expected/$@.mafFrames output/$@.mafFrames

mkout:
	@${MKDIR} output

clean:
	rm -rf output
