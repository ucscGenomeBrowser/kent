include ../../../inc/common.mk

OVERSEL = ../overlapSelect
DIFF = diff -u

test:   pslSelectTests \
	bedSelectTests \
	excludeSelfTests \
	columnSelectTests \
	cdsOverlapTests \
	mergeTests \
	idOutputTests \
	thresholdTests

###
# selecting PSLs
###
pslSelectTests: psl_over_NM_015110gp psl_nonover_NM_015110gp \
		psl_over_NM_015110bed psl_nonover_NM_015110bed psl_over_NM_001206gp \
		psl_nonover_NM_001206gp psl_over_NM_001206bed psl_nonover_NM_001206bed \
		psl_over_NM_015110bed_split psl_nonover_NM_015110bed_split
# select psls overlaping NM_015110 with gp
psl_over_NM_015110gp: mkout
	${OVERSEL} -dropped=output/psl_over_NM_015110gp.drop.psl input/NM_015110.gp input/mrna.psl output/psl_over_NM_015110gp.psl
	${DIFF} expected/psl_over_NM_015110gp.psl output/psl_over_NM_015110gp.psl
	${DIFF} expected/psl_over_NM_015110gp.drop.psl output/psl_over_NM_015110gp.drop.psl

# select psls overlaping not NM_015110 with gp
psl_nonover_NM_015110gp: mkout
	${OVERSEL} -nonOverlapping -dropped=output/psl_nonover_NM_015110gp.drop.psl input/NM_015110.gp input/mrna.psl output/psl_nonover_NM_015110gp.psl
	${DIFF} expected/psl_nonover_NM_015110gp.psl output/psl_nonover_NM_015110gp.psl
	${DIFF} expected/psl_nonover_NM_015110gp.drop.psl output/psl_nonover_NM_015110gp.drop.psl

# select psls overlaping NM_015110 with bed
psl_over_NM_015110bed: mkout
	${OVERSEL} input/NM_015110.bed input/mrna.psl output/psl_over_NM_015110bed.psl
	${DIFF} expected/psl_over_NM_015110bed.psl output/psl_over_NM_015110bed.psl

# select psls overlaping not NM_015110 with bed
psl_nonover_NM_015110bed: mkout
	${OVERSEL} -nonOverlapping input/NM_015110.bed input/mrna.psl output/psl_nonover_NM_015110bed.psl
	${DIFF} expected/psl_nonover_NM_015110bed.psl output/psl_nonover_NM_015110bed.psl

# select psls overlaping NM_001206 with gp
psl_over_NM_001206gp: mkout
	${OVERSEL} input/NM_001206.gp input/mrna.psl output/psl_over_NM_001206gp.psl
	${DIFF} expected/psl_over_NM_001206gp.psl output/psl_over_NM_001206gp.psl

# select psls overlaping not NM_001206 with gp
psl_nonover_NM_001206gp: mkout
	${OVERSEL} -nonOverlapping input/NM_001206.gp input/mrna.psl output/psl_nonover_NM_001206gp.psl
	${DIFF} expected/psl_nonover_NM_001206gp.psl output/psl_nonover_NM_001206gp.psl

# select psls overlaping NM_001206 with bed
psl_over_NM_001206bed: mkout
	${OVERSEL} input/NM_001206.bed input/mrna.psl output/psl_over_NM_001206bed.psl
	${DIFF} expected/psl_over_NM_001206bed.psl output/psl_over_NM_001206bed.psl

# select psls overlaping not NM_001206 with bed
psl_nonover_NM_001206bed: mkout
	${OVERSEL} -nonOverlapping input/NM_001206.bed input/mrna.psl output/psl_nonover_NM_001206bed.psl
	${DIFF} expected/psl_nonover_NM_001206bed.psl output/psl_nonover_NM_001206bed.psl

# select psls overlaping NM_015110 with bed, one block per record
psl_over_NM_015110bed_split: mkout
	${OVERSEL} input/NM_015110.split.bed input/mrna.psl output/psl_over_NM_015110bed_split.psl
	${DIFF} expected/psl_over_NM_015110bed_split.psl output/psl_over_NM_015110bed_split.psl

# select psls overlaping not NM_015110 with bed, one block per record
psl_nonover_NM_015110bed_split: mkout
	${OVERSEL} -nonOverlapping input/NM_015110.split.bed input/mrna.psl output/psl_nonover_NM_015110bed_split.psl
	${DIFF} expected/psl_nonover_NM_015110bed_split.psl output/psl_nonover_NM_015110bed_split.psl

###
# selecting BEDs
###
bedSelectTests: bed_over_NM_015110gp bed_nonover_NM_015110gp \
		bed_over_NM_015110bed bed_nonover_NM_015110bed  \
		bed_over_NM_015110bed_split bed_nonover_NM_015110bed_split \

# select beds overlaping NM_015110 with gp
bed_over_NM_015110gp: mkout
	${OVERSEL} input/NM_015110.gp -dropped=output/bed_over_NM_015110gp.drop.bed input/mrna.bed output/bed_over_NM_015110gp.bed
	${DIFF} expected/bed_over_NM_015110gp.bed output/bed_over_NM_015110gp.bed
	${DIFF} expected/bed_over_NM_015110gp.drop.bed output/bed_over_NM_015110gp.drop.bed

# select beds overlaping not NM_015110 with gp
bed_nonover_NM_015110gp: mkout
	${OVERSEL} -nonOverlapping -dropped=output/bed_nonover_NM_015110gp.drop.bed  input/NM_015110.gp input/mrna.bed output/bed_nonover_NM_015110gp.bed
	${DIFF} expected/bed_nonover_NM_015110gp.bed output/bed_nonover_NM_015110gp.bed
	${DIFF} expected/bed_nonover_NM_015110gp.drop.bed output/bed_nonover_NM_015110gp.drop.bed

# select beds overlaping NM_015110 with bed
bed_over_NM_015110bed: mkout
	${OVERSEL} input/NM_015110.bed input/mrna.bed output/bed_over_NM_015110bed.bed
	${DIFF} expected/bed_over_NM_015110bed.bed output/bed_over_NM_015110bed.bed

# select beds overlaping not NM_015110 with bed
bed_nonover_NM_015110bed: mkout
	${OVERSEL} -nonOverlapping input/NM_015110.bed input/mrna.bed output/bed_nonover_NM_015110bed.bed
	${DIFF} expected/bed_nonover_NM_015110bed.bed output/bed_nonover_NM_015110bed.bed

# select beds overlaping NM_015110 with bed, one block per record
bed_over_NM_015110bed_split: mkout
	${OVERSEL} input/NM_015110.split.bed input/mrna.bed output/bed_over_NM_015110bed_split.bed
	${DIFF} expected/bed_over_NM_015110bed.bed output/bed_over_NM_015110bed.bed

bed_nonover_NM_015110bed_split: mkout
	${OVERSEL} -nonOverlapping input/NM_015110.split.bed input/mrna.bed output/bed_nonover_NM_015110bed_split.bed
	${DIFF} expected/bed_nonover_NM_015110bed_split.bed output/bed_nonover_NM_015110bed_split.bed

###
# -excludeSelf tests
##
excludeSelfTests: exludeself_over_mrna1 exludeself_nonover_mrna1 \
		exludeself_over_mrna2 exludeself_nonover_mrna2


# Test -excludeSelf option with file containing two non-overlaping mRNAs.
# Neither should be selected.
exludeself_over_mrna1: mkout
	${OVERSEL} -excludeSelf input/mrna-self1.psl input/mrna-self1.psl output/exludeself_over_mrna1.psl
	${DIFF} expected/exludeself_over_mrna1.psl output/exludeself_over_mrna1.psl

# Test -excludeSelf and -nonOverlapping options with file containing two
# non-overlaping mRNAs. Both should be selected.
exludeself_nonover_mrna1: mkout
	${OVERSEL} -excludeSelf -nonOverlapping input/mrna-self1.psl input/mrna-self1.psl output/exludeself_nonover_mrna1.psl
	${DIFF} expected/exludeself_nonover_mrna1.psl output/exludeself_nonover_mrna1.psl

# Test of -excludeSelf with file containing 3 overlap mRNAs and 1
# non-overlaping.  Overlaping should be selected, since they are overlapped by
# more than self.
exludeself_over_mrna2: mkout
	${OVERSEL} -excludeSelf input/mrna-self2.psl input/mrna-self2.psl output/exludeself_over_mrna2.psl
	${DIFF} expected/exludeself_over_mrna2.psl output/exludeself_over_mrna2.psl

# Test of -excludeSelf and -nonOverlapping with file containing 3 overlap mRNAs
# and 1 non-overlaping.  Only non-overlaping should be selected.
exludeself_nonover_mrna2: mkout
	${OVERSEL} -excludeSelf -nonOverlapping input/mrna-self2.psl input/mrna-self2.psl output/exludeself_nonover_mrna2.psl
	${DIFF} expected/exludeself_nonover_mrna2.psl output/exludeself_nonover_mrna2.psl

###
# select by column tests
###
columnSelectTests: colSelect colExclude

# select by column
colSelect: mkout
	${OVERSEL} -selectCoordCols=1,2,3 -inCoordCols=3,4,5 \
	    input/gene.select input/gene.clusters output/colSelect.cluster
	${DIFF} expected/colSelect.cluster output/colSelect.cluster

# exclude by column
colExclude: mkout
	${OVERSEL} -nonOverlapping -selectCoordCols=1,2,3 -inCoordCols=3,4,5 \
	    input/gene.select input/gene.clusters output/colExclude.cluster
	${DIFF} expected/colExclude.cluster output/colExclude.cluster

###
# CDS overlap tests
###
cdsOverlapTests: cdsOverlapCdsCds cdsOverlapCdsExon \
		cdsOverlapExonCds noCdsSelect noCdsSelectNoOver

# CDS doesn't overlap
cdsOverlapCdsCds: mkout
	${OVERSEL} -inCds -selectCds input/cdsOverlap1.gp input/cdsOverlap2.gp output/cdsOverlapCdsCds.gp
	${DIFF} expected/cdsOverlapCdsCds.gp output/cdsOverlapCdsCds.gp

cdsOverlapCdsExon: mkout
	${OVERSEL} -inCds input/cdsOverlap1.gp input/cdsOverlap2.gp output/cdsOverlapCdsExon.gp
	${DIFF} expected/cdsOverlapCdsExon.gp output/cdsOverlapCdsExon.gp

cdsOverlapExonCds: mkout
	${OVERSEL} -selectCds input/cdsOverlap1.gp input/cdsOverlap2.gp output/cdsOverlapExonCds.gp
	${DIFF} expected/cdsOverlapExonCds.gp output/cdsOverlapExonCds.gp

# regression: select with CDS and a genePred with no CDS
noCdsSelect: mkout
	${OVERSEL} -selectCds input/noCds.gp input/noCds2.gp output/noCdsSelect.gp
	${DIFF} expected/noCdsSelect.gp output/noCdsSelect.gp
noCdsSelectNoOver: mkout
	${OVERSEL} -nonOverlapping -selectCds input/noCds.gp input/noCds2.gp output/noCdsSelectNoOver.gp
	${DIFF} expected/noCdsSelectNoOver.gp output/noCdsSelectNoOver.gp

###
# -merge tests
###
mergeTests: psl_over_NM_015110gpMerge psl_over_NM_001206bedMerge \
	bed_over_NM_015110bedMerge colSelectMerge

# select psls overlaping NM_015110 with gp and merge
psl_over_NM_015110gpMerge: mkout
	${OVERSEL} -merge -dropped=output/psl_over_NM_015110gpMerge.drop.psl input/NM_015110.gp input/mrna.psl output/psl_over_NM_015110gpMerge.psl
	${DIFF} expected/psl_over_NM_015110gpMerge.psl output/psl_over_NM_015110gpMerge.psl
	${DIFF} expected/psl_over_NM_015110gp.drop.psl output/psl_over_NM_015110gpMerge.drop.psl

# select psls overlaping NM_001206 with bed and merge
psl_over_NM_001206bedMerge: mkout
	${OVERSEL} -merge input/NM_001206.bed input/mrna.psl output/psl_over_NM_001206bedMerge.psl
	${DIFF} expected/psl_over_NM_001206bedMerge.psl output/psl_over_NM_001206bedMerge.psl

# select beds overlaping NM_015110 with bed and merge
bed_over_NM_015110bedMerge: mkout
	${OVERSEL} -merge input/NM_015110.bed input/mrna.bed output/bed_over_NM_015110bedMerge.bed
	${DIFF} expected/bed_over_NM_015110bedMerge.bed output/bed_over_NM_015110bedMerge.bed

# select by column and merge
colSelectMerge: mkout
	${OVERSEL} -merge -selectCoordCols=1,2,3 -inCoordCols=3,4,5 \
	    input/gene.select input/gene.clusters output/colSelectMerge.cluster
	${DIFF} expected/colSelectMerge.cluster output/colSelectMerge.cluster

###
# -idOutput tests
###
idOutputTests: psl_over_NM_015110gpIdOutput psl_over_NM_001206bedIdOutput \
	bed_over_NM_015110bedIdOutput

# select psls overlaping NM_015110 with gp and idOutput
psl_over_NM_015110gpIdOutput: mkout
	${OVERSEL} -idOutput -dropped=output/psl_over_NM_015110gpIdOutput.drop.ids input/NM_015110.gp input/mrna.psl output/psl_over_NM_015110gpIdOutput.ids
	${DIFF} expected/psl_over_NM_015110gpIdOutput.ids output/psl_over_NM_015110gpIdOutput.ids
	${DIFF} expected/psl_over_NM_015110gpIdOutput.drop.ids output/psl_over_NM_015110gpIdOutput.drop.ids

# select psls overlaping NM_001206 with bed and idOutput
psl_over_NM_001206bedIdOutput: mkout
	${OVERSEL} -idOutput input/NM_001206.bed input/mrna.psl output/psl_over_NM_001206bedIdOutput.ids
	${DIFF} expected/psl_over_NM_001206bedIdOutput.ids output/psl_over_NM_001206bedIdOutput.ids

# select beds overlaping NM_015110 with bed and idOutput
bed_over_NM_015110bedIdOutput: mkout
	${OVERSEL} -idOutput input/NM_015110.bed input/mrna.bed output/bed_over_NM_015110bedIdOutput.ids
	${DIFF} expected/bed_over_NM_015110bedIdOutput.ids output/bed_over_NM_015110bedIdOutput.ids

###
# threshold tests
###
thresholdTests: psl_gp_threshold  psl_gp_non_threshold \
	gp_gp_threshold95 gp_gp_threshold75 \
	gp_gp_thresholdCds95 gp_gp_thresholdCds75

# select psls overlaping NM_015110 with gp and overlapThreshold
psl_gp_threshold: mkout
	${OVERSEL} -overlapThreshold=0.4 -dropped=output/psl_gp_threshold.drop.psl input/NM_015110.gp input/mrna.psl output/psl_gp_threshold.psl
	${DIFF} expected/psl_gp_threshold.psl output/psl_gp_threshold.psl
	${DIFF} expected/psl_gp_threshold.drop.psl output/psl_gp_threshold.drop.psl

# select psls overlaping not NM_015110 with gp and overlapThreshold
psl_gp_non_threshold: mkout
	${OVERSEL} -overlapThreshold=0.4 -nonOverlapping -dropped=output/psl_gp_non_threshold.drop.psl input/NM_015110.gp input/mrna.psl output/psl_gp_non_threshold.psl
	${DIFF} expected/psl_gp_non_threshold.psl output/psl_gp_non_threshold.psl
	${DIFF} expected/psl_gp_non_threshold.drop.psl output/psl_gp_non_threshold.drop.psl

# 95% threshold
gp_gp_threshold95: mkout
	${OVERSEL} -idOutput -overlapThreshold=0.95 input/NM_015110.gp  input/NM_015110Similar.gp output/gp_gp_threshold95.ids
	${OVERSEL} -idOutput -overlapThreshold=0.95 input/NM_015110Similar.gp input/NM_015110.gp output/gp_gp_threshold95Rev.ids
	${DIFF} expected/gp_gp_threshold95.ids output/gp_gp_threshold95.ids
	${DIFF} expected/gp_gp_threshold95Rev.ids output/gp_gp_threshold95Rev.ids

# 75% threshold
gp_gp_threshold75: mkout
	${OVERSEL} -idOutput -overlapThreshold=0.75 input/NM_015110.gp  input/NM_015110Similar.gp output/gp_gp_threshold75.ids
	${OVERSEL} -idOutput -overlapThreshold=0.75 input/NM_015110Similar.gp input/NM_015110.gp output/gp_gp_threshold75Rev.ids
	${DIFF} expected/gp_gp_threshold75.ids output/gp_gp_threshold75.ids
	${DIFF} expected/gp_gp_threshold75Rev.ids output/gp_gp_threshold75Rev.ids

# 95% CDS threshold
gp_gp_thresholdCds95: mkout
	${OVERSEL} -idOutput -overlapThreshold=0.95 -selectCds -inCds input/NM_015110.gp  input/NM_015110Similar.gp output/gp_gp_thresholdCds95.ids
	${OVERSEL} -idOutput -overlapThreshold=0.95 -selectCds -inCds input/NM_015110Similar.gp input/NM_015110.gp output/gp_gp_thresholdCds95Rev.ids
	${DIFF} expected/gp_gp_thresholdCds95.ids output/gp_gp_thresholdCds95.ids
	${DIFF} expected/gp_gp_thresholdCds95Rev.ids output/gp_gp_thresholdCds95Rev.ids

# 75% CDS threshold
gp_gp_thresholdCds75: mkout
	${OVERSEL} -idOutput -overlapThreshold=0.75 -selectCds -inCds input/NM_015110.gp  input/NM_015110Similar.gp output/gp_gp_thresholdCds75.ids
	${OVERSEL} -idOutput -overlapThreshold=0.75 -selectCds -inCds input/NM_015110Similar.gp input/NM_015110.gp output/gp_gp_thresholdCds75Rev.ids
	${DIFF} expected/gp_gp_thresholdCds75.ids output/gp_gp_thresholdCds75.ids
	${DIFF} expected/gp_gp_thresholdCds75Rev.ids output/gp_gp_thresholdCds75Rev.ids

###
# similarity tests
###
similarityTests: psl_gp_similarity  psl_gp_non_similarity \
	gp_gp_similarity95 gp_gp_similarity75 \
	gp_gp_similarityCds95 gp_gp_similarityCds75

# select psls overlaping NM_015110 with gp and overlapSimilarity
psl_gp_similarity: mkout
	${OVERSEL} -overlapSimilarity=0.4 -dropped=output/psl_gp_similarity.drop.psl input/NM_015110.gp input/mrna.psl output/psl_gp_similarity.psl
	${DIFF} expected/psl_gp_similarity.psl output/psl_gp_similarity.psl
	${DIFF} expected/psl_gp_similarity.drop.psl output/psl_gp_similarity.drop.psl

# select psls overlaping not NM_015110 with gp and overlapSimilarity
psl_gp_non_similarity: mkout
	${OVERSEL} -overlapSimilarity=0.4 -nonOverlapping -dropped=output/psl_gp_non_similarity.drop.psl input/NM_015110.gp input/mrna.psl output/psl_gp_non_similarity.psl
	${DIFF} expected/psl_gp_non_similarity.psl output/psl_gp_non_similarity.psl
	${DIFF} expected/psl_gp_non_similarity.drop.psl output/psl_gp_non_similarity.drop.psl

# 95% similarity
gp_gp_similarity95: mkout
	${OVERSEL} -idOutput -overlapSimilarity=0.95 input/NM_015110.gp  input/NM_015110Similar.gp output/gp_gp_similarity95.ids
	${OVERSEL} -idOutput -overlapSimilarity=0.95 input/NM_015110Similar.gp input/NM_015110.gp output/gp_gp_similarity95Rev.ids
	${DIFF} expected/gp_gp_similarity95.ids output/gp_gp_similarity95.ids
	${DIFF} expected/gp_gp_similarity95Rev.ids output/gp_gp_similarity95Rev.ids

# 75% similarity
gp_gp_similarity75: mkout
	${OVERSEL} -idOutput -overlapSimilarity=0.75 input/NM_015110.gp  input/NM_015110Similar.gp output/gp_gp_similarity75.ids
	${OVERSEL} -idOutput -overlapSimilarity=0.75 input/NM_015110Similar.gp input/NM_015110.gp output/gp_gp_similarity75Rev.ids
	${DIFF} expected/gp_gp_similarity75.ids output/gp_gp_similarity75.ids
	${DIFF} expected/gp_gp_similarity75Rev.ids output/gp_gp_similarity75Rev.ids

# 95% CDS similarity
gp_gp_similarityCds95: mkout
	${OVERSEL} -idOutput -overlapSimilarity=0.95 -selectCds -inCds input/NM_015110.gp  input/NM_015110Similar.gp output/gp_gp_similarityCds95.ids
	${OVERSEL} -idOutput -overlapSimilarity=0.95 -selectCds -inCds input/NM_015110Similar.gp input/NM_015110.gp output/gp_gp_similarityCds95Rev.ids
	${DIFF} expected/gp_gp_similarityCds95.ids output/gp_gp_similarityCds95.ids
	${DIFF} expected/gp_gp_similarityCds95Rev.ids output/gp_gp_similarityCds95Rev.ids

# 75% CDS similarity
gp_gp_similarityCds75: mkout
	${OVERSEL} -idOutput -overlapSimilarity=0.75 -selectCds -inCds input/NM_015110.gp  input/NM_015110Similar.gp output/gp_gp_similarityCds75.ids
	${OVERSEL} -idOutput -overlapSimilarity=0.75 -selectCds -inCds input/NM_015110Similar.gp input/NM_015110.gp output/gp_gp_similarityCds75Rev.ids
	${DIFF} expected/gp_gp_similarityCds75.ids output/gp_gp_similarityCds75.ids
	${DIFF} expected/gp_gp_similarityCds75Rev.ids output/gp_gp_similarityCds75Rev.ids

mkout:
	@${MKDIR} output

clean:
	rm -rf output
