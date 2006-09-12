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
	statsOutputTests \
	thresholdTests \
	similarityTests \
	overlapBasesTests \
	aggregateTests \
	protTests \
	idMatchTests \
	rangeTests \
	cds_id_out_gp \
	wideTest

###
# selecting PSLs
###
pslSelectTests: psl_over_NM_015110gp psl_nonover_NM_015110gp \
		psl_over_NM_015110bed psl_nonover_NM_015110bed psl_over_NM_001206gp \
		psl_nonover_NM_001206gp psl_over_NM_001206bed psl_nonover_NM_001206bed \
		psl_over_NM_015110bed_split psl_nonover_NM_015110bed_split

# select psls overlaping NM_015110 with gp
psl_over_NM_015110gp: mkout
	${OVERSEL} -dropped=output/$@.drop.psl input/NM_015110.gp input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl
	${DIFF} expected/$@.drop.psl output/$@.drop.psl

# select psls overlaping not NM_015110 with gp
psl_nonover_NM_015110gp: mkout
	${OVERSEL} -nonOverlapping -dropped=output/$@.drop.psl input/NM_015110.gp input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl
	${DIFF} expected/$@.drop.psl output/$@.drop.psl

# select psls overlaping NM_015110 with bed
psl_over_NM_015110bed: mkout
	${OVERSEL} input/NM_015110.bed input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl

# select psls overlaping not NM_015110 with bed
psl_nonover_NM_015110bed: mkout
	${OVERSEL} -nonOverlapping input/NM_015110.bed input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl

# select psls overlaping NM_001206 with gp
psl_over_NM_001206gp: mkout
	${OVERSEL} input/NM_001206.gp input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl

# select psls overlaping not NM_001206 with gp
psl_nonover_NM_001206gp: mkout
	${OVERSEL} -nonOverlapping input/NM_001206.gp input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl

# select psls overlaping NM_001206 with bed
psl_over_NM_001206bed: mkout
	${OVERSEL} input/NM_001206.bed input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl

# select psls overlaping not NM_001206 with bed
psl_nonover_NM_001206bed: mkout
	${OVERSEL} -nonOverlapping input/NM_001206.bed input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl

# select psls overlaping NM_015110 with bed, one block per record
psl_over_NM_015110bed_split: mkout
	${OVERSEL} input/NM_015110.split.bed input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl

# select psls overlaping not NM_015110 with bed, one block per record
psl_nonover_NM_015110bed_split: mkout
	${OVERSEL} -nonOverlapping input/NM_015110.split.bed input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl

###
# selecting BEDs
###
bedSelectTests: bed_over_NM_015110gp bed_nonover_NM_015110gp \
		bed_over_NM_015110bed bed_nonover_NM_015110bed  \
		bed_over_NM_015110bed_split bed_nonover_NM_015110bed_split \

# select beds overlaping NM_015110 with gp
bed_over_NM_015110gp: mkout
	${OVERSEL} input/NM_015110.gp -dropped=output/$@.drop.bed input/mrna.bed output/$@.bed
	${DIFF} expected/$@.bed output/$@.bed
	${DIFF} expected/$@.drop.bed output/$@.drop.bed

# select beds overlaping not NM_015110 with gp
bed_nonover_NM_015110gp: mkout
	${OVERSEL} -nonOverlapping -dropped=output/$@.drop.bed  input/NM_015110.gp input/mrna.bed output/$@.bed
	${DIFF} expected/$@.bed output/$@.bed
	${DIFF} expected/$@.drop.bed output/$@.drop.bed

# select beds overlaping NM_015110 with bed
bed_over_NM_015110bed: mkout
	${OVERSEL} input/NM_015110.bed input/mrna.bed output/$@.bed
	${DIFF} expected/$@.bed output/$@.bed

# select beds overlaping not NM_015110 with bed
bed_nonover_NM_015110bed: mkout
	${OVERSEL} -nonOverlapping input/NM_015110.bed input/mrna.bed output/$@.bed
	${DIFF} expected/$@.bed output/$@.bed

# select beds overlaping NM_015110 with bed, one block per record
bed_over_NM_015110bed_split: mkout
	${OVERSEL} input/NM_015110.split.bed input/mrna.bed output/$@.bed
	${DIFF} expected/$@.bed output/$@.bed

bed_nonover_NM_015110bed_split: mkout
	${OVERSEL} -nonOverlapping input/NM_015110.split.bed input/mrna.bed output/$@.bed
	${DIFF} expected/$@.bed output/$@.bed

###
# -excludeSelf tests
##
excludeSelfTests: exludeself_over_mrna1 exludeself_nonover_mrna1 \
		exludeself_over_mrna2 exludeself_nonover_mrna2

# Test -excludeSelf option with file containing two non-overlaping mRNAs.
# Neither should be selected.
exludeself_over_mrna1: mkout
	${OVERSEL} -excludeSelf input/mrna-self1.psl input/mrna-self1.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl

# Test -excludeSelf and -nonOverlapping options with file containing two
# non-overlaping mRNAs. Both should be selected.
exludeself_nonover_mrna1: mkout
	${OVERSEL} -excludeSelf -nonOverlapping input/mrna-self1.psl input/mrna-self1.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl

# Test of -excludeSelf with file containing 3 overlap mRNAs and 1
# non-overlaping.  Overlaping should be selected, since they are overlapped by
# more than self.
exludeself_over_mrna2: mkout
	${OVERSEL} -excludeSelf input/mrna-self2.psl input/mrna-self2.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl

# Test of -excludeSelf and -nonOverlapping with file containing 3 overlap mRNAs
# and 1 non-overlaping.  Only non-overlaping should be selected.
exludeself_nonover_mrna2: mkout
	${OVERSEL} -excludeSelf -nonOverlapping input/mrna-self2.psl input/mrna-self2.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl

###
# select by column tests
###
columnSelectTests: colSelect colExclude

# select by column
colSelect: mkout
	${OVERSEL} -selectCoordCols=1,2,3 -inCoordCols=3,4,5 \
	    input/gene.select input/gene.clusters output/$@.cluster
	${DIFF} expected/$@.cluster output/$@.cluster

# exclude by column
colExclude: mkout
	${OVERSEL} -nonOverlapping -selectCoordCols=1,2,3 -inCoordCols=3,4,5 \
	    input/gene.select input/gene.clusters output/$@.cluster
	${DIFF} expected/$@.cluster output/$@.cluster

# select by column, including strand
colSelectStrand: mkout
	${OVERSEL} -strand -selectCoordCols=1,2,3,4 -inCoordCols=3,4,5,6 \
	    input/gene.select input/gene.clusters output/$@.cluster
	${DIFF} expected/$@.cluster output/$@.cluster

###
# CDS overlap tests
###
cdsOverlapTests: cdsOverlapCdsCds cdsOverlapCdsExon \
		cdsOverlapExonCds noCdsSelect noCdsSelectNoOver

# CDS doesn't overlap
cdsOverlapCdsCds: mkout
	${OVERSEL} -inCds -selectCds input/cdsOverlap1.gp input/cdsOverlap2.gp output/$@.gp
	${DIFF} expected/$@.gp output/$@.gp

cdsOverlapCdsExon: mkout
	${OVERSEL} -inCds input/cdsOverlap1.gp input/cdsOverlap2.gp output/$@.gp
	${DIFF} expected/$@.gp output/$@.gp

cdsOverlapExonCds: mkout
	${OVERSEL} -selectCds input/cdsOverlap1.gp input/cdsOverlap2.gp output/$@.gp
	${DIFF} expected/$@.gp output/$@.gp

# regression: select with CDS and a genePred with no CDS
noCdsSelect: mkout
	${OVERSEL} -selectCds input/noCds.gp input/noCds2.gp output/$@.gp
	${DIFF} expected/$@.gp output/$@.gp
noCdsSelectNoOver: mkout
	${OVERSEL} -nonOverlapping -selectCds input/noCds.gp input/noCds2.gp output/$@.gp
	${DIFF} expected/$@.gp output/$@.gp

###
# -merge tests
###
mergeTests: psl_over_NM_015110gpMerge psl_over_NM_001206bedMerge \
	bed_over_NM_015110bedMerge colSelectMerge

# select psls overlaping NM_015110 with gp and merge
psl_over_NM_015110gpMerge: mkout
	${OVERSEL} -mergeOutput -dropped=output/$@.drop.psl input/NM_015110.gp input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl
	${DIFF} expected/$@.drop.psl output/$@.drop.psl

# select psls overlaping NM_001206 with bed and merge
psl_over_NM_001206bedMerge: mkout
	${OVERSEL} -mergeOutput input/NM_001206.bed input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl

# select beds overlaping NM_015110 with bed and merge
bed_over_NM_015110bedMerge: mkout
	${OVERSEL} -mergeOutput input/NM_015110.bed input/mrna.bed output/$@.bed
	${DIFF} expected/$@.bed output/$@.bed

# select by column and merge
colSelectMerge: mkout
	${OVERSEL} -mergeOutput -selectCoordCols=1,2,3 -inCoordCols=3,4,5 \
	    input/gene.select input/gene.clusters output/$@.cluster
	${DIFF} expected/$@.cluster output/$@.cluster

###
# -idOutput tests
###
idOutputTests: psl_over_NM_015110gpIdOutput psl_over_NM_001206bedIdOutput \
	bed_over_NM_015110bedIdOutput

# select psls overlaping NM_015110 with gp and idOutput
psl_over_NM_015110gpIdOutput: mkout
	${OVERSEL} -idOutput -dropped=output/$@.drop.ids input/NM_015110.gp input/mrna.psl output/$@.ids
	${DIFF} expected/$@.ids output/$@.ids
	${DIFF} expected/$@.drop.ids output/$@.drop.ids

# select psls overlaping NM_001206 with bed and idOutput
psl_over_NM_001206bedIdOutput: mkout
	${OVERSEL} -idOutput input/NM_001206.bed input/mrna.psl output/$@.ids
	${DIFF} expected/$@.ids output/$@.ids

# select beds overlaping NM_015110 with bed and idOutput
bed_over_NM_015110bedIdOutput: mkout
	${OVERSEL} -idOutput input/NM_015110.bed input/mrna.bed output/$@.ids
	${DIFF} expected/$@.ids output/$@.ids

###
# -statsOutput tests
###
statsOutputTests: psl_over_NM_015110gpStatsOutput psl_over_NM_001206bedStatsOutput \
	bed_over_NM_015110bedStatsOutput

# select psls overlaping NM_015110 with gp and statsOutput
psl_over_NM_015110gpStatsOutput: mkout
	${OVERSEL} -statsOutput input/NM_015110.gp input/mrna.psl output/$@.stats
	${DIFF} expected/$@.stats output/$@.stats

# select psls overlaping NM_001206 with bed and statsOutput
psl_over_NM_001206bedStatsOutput: mkout
	${OVERSEL} -statsOutput input/NM_001206.bed input/mrna.psl output/$@.stats
	${DIFF} expected/$@.stats output/$@.stats

# select beds overlaping NM_015110 with bed and statsOutput
bed_over_NM_015110bedStatsOutput: mkout
	${OVERSEL} -statsOutput input/NM_015110.bed input/mrna.bed output/$@.stats
	${DIFF} expected/$@.stats output/$@.stats

###
# threshold tests
###
thresholdTests: psl_gp_threshold  psl_gp_non_threshold \
	gp_gp_threshold95 gp_gp_threshold75 gp_gp_threshold100 \
	gp_gp_thresholdCds95 gp_gp_thresholdCds75 gp_gp_thresholdCds100 \
	psl_gp_threshold_ceil

# select psls overlaping NM_015110 with gp and overlapThreshold
psl_gp_threshold: mkout
	${OVERSEL} -overlapThreshold=0.4 -dropped=output/$@.drop.psl input/NM_015110.gp input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl
	${DIFF} expected/$@.drop.psl output/$@.drop.psl

# select psls overlaping not NM_015110 with gp and overlapThreshold
psl_gp_non_threshold: mkout
	${OVERSEL} -overlapThreshold=0.4 -nonOverlapping -dropped=output/$@.drop.psl input/NM_015110.gp input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl
	${DIFF} expected/$@.drop.psl output/$@.drop.psl

# 95% threshold
gp_gp_threshold95: mkout
	${OVERSEL} -idOutput -overlapThreshold=0.95 input/NM_015110.gp  input/NM_015110Similar.gp output/$@.ids
	${OVERSEL} -idOutput -overlapThreshold=0.95 input/NM_015110Similar.gp input/NM_015110.gp output/$@Rev.ids
	${DIFF} expected/$@.ids output/$@.ids
	${DIFF} expected/$@Rev.ids output/$@Rev.ids

# 75% threshold
gp_gp_threshold75: mkout
	${OVERSEL} -idOutput -overlapThreshold=0.75 input/NM_015110.gp  input/NM_015110Similar.gp output/$@.ids
	${OVERSEL} -idOutput -overlapThreshold=0.75 input/NM_015110Similar.gp input/NM_015110.gp output/$@Rev.ids
	${DIFF} expected/$@.ids output/$@.ids
	${DIFF} expected/$@Rev.ids output/$@Rev.ids

# 100% threshold
gp_gp_threshold100: mkout
	${OVERSEL} -idOutput -overlapThreshold=1.0 input/NM_015110.gp  input/NM_015110Similar.gp output/$@.ids
	${OVERSEL} -idOutput -overlapThreshold=1.0 input/NM_015110Similar.gp input/NM_015110.gp output/$@Rev.ids
	${DIFF} expected/$@.ids output/$@.ids
	${DIFF} expected/$@Rev.ids output/$@Rev.ids

# 95% CDS threshold
gp_gp_thresholdCds95: mkout
	${OVERSEL} -idOutput -overlapThreshold=0.95 -selectCds -inCds input/NM_015110.gp  input/NM_015110Similar.gp output/$@.ids
	${OVERSEL} -idOutput -overlapThreshold=0.95 -selectCds -inCds input/NM_015110Similar.gp input/NM_015110.gp output/$@Rev.ids
	${DIFF} expected/$@.ids output/$@.ids
	${DIFF} expected/$@Rev.ids output/$@Rev.ids

# 75% CDS threshold
gp_gp_thresholdCds75: mkout
	${OVERSEL} -idOutput -overlapThreshold=0.75 -selectCds -inCds input/NM_015110.gp  input/NM_015110Similar.gp output/$@.ids
	${OVERSEL} -idOutput -overlapThreshold=0.75 -selectCds -inCds input/NM_015110Similar.gp input/NM_015110.gp output/$@Rev.ids
	${DIFF} expected/$@.ids output/$@.ids
	${DIFF} expected/$@Rev.ids output/$@Rev.ids

# 100% CDS threshold
gp_gp_thresholdCds100: mkout
	${OVERSEL} -idOutput -overlapThreshold=1.0 -selectCds -inCds input/NM_015110.gp  input/NM_015110Similar.gp output/$@.ids
	${OVERSEL} -idOutput -overlapThreshold=1.0 -selectCds -inCds input/NM_015110Similar.gp input/NM_015110.gp output/$@Rev.ids
	${DIFF} expected/$@.ids output/$@.ids
	${DIFF} expected/$@Rev.ids output/$@Rev.ids

# select psls overlaping NM_015110 with gp, overlapThreshold, overlapThresholdCeil
psl_gp_threshold_ceil: mkout
	${OVERSEL} -overlapThreshold=0.4 -overlapThresholdCeil=0.6 -dropped=output/$@.drop.psl input/NM_015110.gp input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl
	${DIFF} expected/$@.drop.psl output/$@.drop.psl

###
# similarity tests
###
similarityTests: psl_gp_similarity  psl_gp_non_similarity \
	gp_gp_similarity95 gp_gp_similarity75 gp_gp_similarity100 \
	gp_gp_similarityCds95 gp_gp_similarityCds75 gp_gp_similarityCds100 \
	psl_gp_similarity_ceil

# select psls overlaping NM_015110 with gp and overlapSimilarity
psl_gp_similarity: mkout
	${OVERSEL} -overlapSimilarity=0.4 -dropped=output/$@.drop.psl input/NM_015110.gp input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl
	${DIFF} expected/$@.drop.psl output/$@.drop.psl

# select psls overlaping not NM_015110 with gp and overlapSimilarity
psl_gp_non_similarity: mkout
	${OVERSEL} -overlapSimilarity=0.4 -nonOverlapping -dropped=output/$@.drop.psl input/NM_015110.gp input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl
	${DIFF} expected/$@.drop.psl output/$@.drop.psl

# 95% similarity
gp_gp_similarity95: mkout
	${OVERSEL} -idOutput -overlapSimilarity=0.95 input/NM_015110.gp  input/NM_015110Similar.gp output/$@.ids
	${OVERSEL} -idOutput -overlapSimilarity=0.95 input/NM_015110Similar.gp input/NM_015110.gp output/$@Rev.ids
	${DIFF} expected/$@.ids output/$@.ids
	${DIFF} expected/$@Rev.ids output/$@Rev.ids

# 75% similarity
gp_gp_similarity75: mkout
	${OVERSEL} -idOutput -overlapSimilarity=0.75 input/NM_015110.gp  input/NM_015110Similar.gp output/$@.ids
	${OVERSEL} -idOutput -overlapSimilarity=0.75 input/NM_015110Similar.gp input/NM_015110.gp output/$@Rev.ids
	${DIFF} expected/$@.ids output/$@.ids
	${DIFF} expected/$@Rev.ids output/$@Rev.ids

# 100% similarity
gp_gp_similarity100: mkout
	${OVERSEL} -idOutput -overlapSimilarity=1.0 input/NM_015110.gp  input/NM_015110Similar.gp output/$@.ids
	${OVERSEL} -idOutput -overlapSimilarity=1.0 input/NM_015110Similar.gp input/NM_015110.gp output/$@Rev.ids
	${DIFF} expected/$@.ids output/$@.ids
	${DIFF} expected/$@Rev.ids output/$@Rev.ids

# 95% CDS similarity
gp_gp_similarityCds95: mkout
	${OVERSEL} -idOutput -overlapSimilarity=0.95 -selectCds -inCds input/NM_015110.gp  input/NM_015110Similar.gp output/$@.ids
	${OVERSEL} -idOutput -overlapSimilarity=0.95 -selectCds -inCds input/NM_015110Similar.gp input/NM_015110.gp output/$@Rev.ids
	${DIFF} expected/$@.ids output/$@.ids
	${DIFF} expected/$@Rev.ids output/$@Rev.ids

# 75% CDS similarity
gp_gp_similarityCds75: mkout
	${OVERSEL} -idOutput -overlapSimilarity=0.75 -selectCds -inCds input/NM_015110.gp  input/NM_015110Similar.gp output/$@.ids
	${OVERSEL} -idOutput -overlapSimilarity=0.75 -selectCds -inCds input/NM_015110Similar.gp input/NM_015110.gp output/$@Rev.ids
	${DIFF} expected/$@.ids output/$@.ids
	${DIFF} expected/$@Rev.ids output/$@Rev.ids

# 100% CDS similarity
gp_gp_similarityCds100: mkout
	${OVERSEL} -idOutput -overlapSimilarity=1.0 -selectCds -inCds input/NM_015110.gp  input/NM_015110Similar.gp output/$@.ids
	${OVERSEL} -idOutput -overlapSimilarity=1.0 -selectCds -inCds input/NM_015110Similar.gp input/NM_015110.gp output/$@Rev.ids
	${DIFF} expected/$@.ids output/$@.ids
	${DIFF} expected/$@Rev.ids output/$@Rev.ids

# select psls overlaping NM_015110 with gp, overlapSimilarity, and  overlapSimilarityCeil
psl_gp_similarity_ceil: mkout
	${OVERSEL} -overlapSimilarity=0.4 -overlapSimilarityCeil=0.75 -dropped=output/$@.drop.psl input/NM_015110.gp input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl
	${DIFF} expected/$@.drop.psl output/$@.drop.psl

###
# -overlapBases tests
###
overlapBasesTests: overlapBases250 overlapBases2100 overlapBases3000

overlapBases250: mkout
	${OVERSEL} -overlapBases=250 input/NM_015110.bed input/mrna.bed output/$@.bed
	${DIFF} expected/$@.bed output/$@.bed

overlapBases2100: mkout
	${OVERSEL} -overlapBases=2100 input/NM_015110.bed input/mrna.bed output/$@.bed
	${DIFF} expected/$@.bed output/$@.bed

overlapBases3000: mkout
	${OVERSEL} -overlapBases=3000 input/NM_015110.bed input/mrna.bed output/$@.bed
	${DIFF} expected/$@.bed output/$@.bed

###
# aggregate threshold tests
###
aggregateTests: psl_gp_aggregate  psl_gp_non_aggregate \
	gp_gp_aggregate95 gp_gp_aggregate75 gp_gp_aggregate100 \
	gp_gp_aggregateCds95 gp_gp_aggregateCds75 gp_gp_aggregateCds100 \
	gp_gp_aggregateStats psl_gp_aggregateStatsAll

# select psls overlaping NM_015110 with gp and overlapThreshold
psl_gp_aggregate: mkout
	${OVERSEL} -aggregate -overlapThreshold=0.4 -dropped=output/$@.drop.psl input/NM_015110.gp input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl
	${DIFF} expected/$@.drop.psl output/$@.drop.psl

# select psls overlaping not NM_015110 with gp and overlapThreshold
psl_gp_non_aggregate: mkout
	${OVERSEL} -aggregate -overlapThreshold=0.4 -nonOverlapping -dropped=output/$@.drop.psl input/NM_015110.gp input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl
	${DIFF} expected/$@.drop.psl output/$@.drop.psl

# 95% aggregate
gp_gp_aggregate95: mkout
	${OVERSEL} -aggregate -idOutput -overlapThreshold=0.95 input/NM_015110.gp  input/NM_015110Similar.gp output/$@.ids
	${OVERSEL} -aggregate -idOutput -overlapThreshold=0.95 input/NM_015110Similar.gp input/NM_015110.gp output/$@Rev.ids
	${DIFF} expected/$@.ids output/$@.ids
	${DIFF} expected/$@Rev.ids output/$@Rev.ids

# 75% aggregate
gp_gp_aggregate75: mkout
	${OVERSEL} -aggregate -idOutput -overlapThreshold=0.75 input/NM_015110.gp  input/NM_015110Similar.gp output/$@.ids
	${OVERSEL} -aggregate -idOutput -overlapThreshold=0.75 input/NM_015110Similar.gp input/NM_015110.gp output/$@Rev.ids
	${DIFF} expected/$@.ids output/$@.ids
	${DIFF} expected/$@Rev.ids output/$@Rev.ids

# 100% aggregate
gp_gp_aggregate100: mkout
	${OVERSEL} -aggregate -idOutput -overlapThreshold=1.0 input/NM_015110.gp  input/NM_015110Similar.gp output/$@.ids
	${OVERSEL} -aggregate -idOutput -overlapThreshold=1.0 input/NM_015110Similar.gp input/NM_015110.gp output/$@Rev.ids
	${DIFF} expected/$@.ids output/$@.ids
	${DIFF} expected/$@Rev.ids output/$@Rev.ids

# 95% CDS aggregate
gp_gp_aggregateCds95: mkout
	${OVERSEL} -aggregate -idOutput -overlapThreshold=0.95 -selectCds -inCds input/NM_015110.gp  input/NM_015110Similar.gp output/$@.ids
	${OVERSEL} -aggregate -idOutput -overlapThreshold=0.95 -selectCds -inCds input/NM_015110Similar.gp input/NM_015110.gp output/$@Rev.ids
	${DIFF} expected/$@.ids output/$@.ids
	${DIFF} expected/$@Rev.ids output/$@Rev.ids

# 75% CDS aggregate
gp_gp_aggregateCds75: mkout
	${OVERSEL} -aggregate -idOutput -overlapThreshold=0.75 -selectCds -inCds input/NM_015110.gp  input/NM_015110Similar.gp output/$@.ids
	${OVERSEL} -aggregate -idOutput -overlapThreshold=0.75 -selectCds -inCds input/NM_015110Similar.gp input/NM_015110.gp output/$@Rev.ids
	${DIFF} expected/$@.ids output/$@.ids
	${DIFF} expected/$@Rev.ids output/$@Rev.ids

# 100% CDS aggregate
gp_gp_aggregateCds100: mkout
	${OVERSEL} -aggregate -idOutput -overlapThreshold=1.0 -selectCds -inCds input/NM_015110.gp  input/NM_015110Similar.gp output/$@.ids
	${OVERSEL} -aggregate -idOutput -overlapThreshold=1.0 -selectCds -inCds input/NM_015110Similar.gp input/NM_015110.gp output/$@Rev.ids
	${DIFF} expected/$@.ids output/$@.ids
	${DIFF} expected/$@Rev.ids output/$@Rev.ids

# aggregate stats
gp_gp_aggregateStats: mkout
	${OVERSEL} -aggregate -statsOutput input/NM_015110.gp  input/NM_015110Similar.gp output/$@.stats
	${DIFF} expected/$@.stats output/$@.stats

# aggregate stats on all records
psl_gp_aggregateStatsAll: mkout
	${OVERSEL} -aggregate -statsOutputAll input/NM_015110.gp input/mrna.psl output/$@.stats
	${DIFF} expected/$@.stats output/$@.stats


###
# protein tests
###
protTests: gp_psl_prot80 gp_psl_protStats psl_gp_prot80 psl_gp_protStats

# select from a protein psl with a genePred, using only cds of genePred.
# since this uses similarity, it will only select if the psl has been
# translated to query base coords, otherwise it appears to be 1/3 the size.
# Also checks if the strand was correctly translated.
gp_psl_prot80: mkout
	${OVERSEL} -strand -overlapSimilarity=0.9 -selectCds input/refGeneForProt.gp  input/prot.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl

gp_psl_protStats: mkout
	${OVERSEL} -statsOutput -strand -selectCds input/refGeneForProt.gp  input/prot.psl output/$@.stats
	${DIFF} expected/$@.stats output/$@.stats

psl_gp_prot80: mkout
	${OVERSEL} -strand -overlapSimilarity=0.9 -inCds input/prot.psl input/refGeneForProt.gp  output/$@.gp
	${DIFF} expected/$@.gp output/$@.gp

psl_gp_protStats: mkout
	${OVERSEL} -statsOutput -strand -inCds input/prot.psl input/refGeneForProt.gp output/$@.stats
	${DIFF} expected/$@.stats output/$@.stats

###
# idMatch tests
###
idMatchTests: psl_idMatch psl_idMatch_non

psl_idMatch: mkout
	${OVERSEL} -idMatch -overlapSimilarity=0.8 -dropped=output/$@.drop.psl input/mrna-idMatch1.psl input/mrna-idMatch2.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl
	${DIFF} expected/$@.drop.psl output/$@.drop.psl

psl_idMatch_non: mkout
	${OVERSEL} -idMatch -overlapSimilarity=0.8 -nonOverlapping -dropped=output/$@.drop.psl input/mrna-idMatch1.psl input/mrna-idMatch2.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl
	${DIFF} expected/$@.drop.psl output/$@.drop.psl

###
# whole range overlap
###
rangeTests: range_psl_over_NM_015110gp range_psl_nonover_NM_015110gp \
	    range_psl_over_NM_015110bed range_psl_nonover_NM_015110bed \
	    range_NM_015110gp_over_psl range_NM_015110gp_nonover_psl \
	    range_NM_015110bed_over_psl range_NM_015110bed_nonover_psl \
	    range_nostrand_gp_gp range_strand_gp_gp \
	    range_nostrand_gp_nonover_gp range_strand_gp_nonover_gp

# range select psls overlaping NM_015110 with gp
range_psl_over_NM_015110gp: mkout
	${OVERSEL} -selectRange -dropped=output/$@.drop.psl input/NM_015110.gp input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl
	${DIFF} expected/$@.drop.psl output/$@.drop.psl

# range select psls overlaping not NM_015110 with gp
range_psl_nonover_NM_015110gp: mkout
	${OVERSEL} -selectRange -nonOverlapping -dropped=output/$@.drop.psl input/NM_015110.gp input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl
	${DIFF} expected/$@.drop.psl output/$@.drop.psl

# range select psls overlaping NM_015110 with bed
range_psl_over_NM_015110bed: mkout
	${OVERSEL} -selectRange input/NM_015110.bed input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl

# range select psls overlaping not NM_015110 with bed
range_psl_nonover_NM_015110bed: mkout
	${OVERSEL} -selectRange -nonOverlapping input/NM_015110.bed input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl

# range select genePred overlapping psls
range_NM_015110gp_over_psl: mkout
	${OVERSEL} -inRange input/inNM_015110.psl input/NM_015110.gp output/$@.gp
	${DIFF} expected/$@.gp output/$@.gp

# range select genePred not overlapping psls
range_NM_015110gp_nonover_psl: mkout
	${OVERSEL} -inRange -nonOverlapping input/inNM_015110.psl input/NM_015110.gp output/$@.gp
	${DIFF} expected/$@.gp output/$@.gp

# range select bed overlaping psls
range_NM_015110bed_over_psl: mkout
	${OVERSEL} -inRange input/inNM_015110.psl input/NM_015110.bed output/$@.bed
	${DIFF} expected/$@.bed output/$@.bed

# range select psls overlaping not NM_015110 with bed
range_NM_015110bed_nonover_psl: mkout
	${OVERSEL} -inRange -nonOverlapping input/inNM_015110.psl input/NM_015110.bed output/$@.bed
	${DIFF} expected/$@.bed output/$@.bed

# -selectRange contained in intron on both strands (without -strand)
range_nostrand_gp_gp: mkout
	${OVERSEL} -selectRange input/rangeSel1.gp input/rangeIn1.gp output/$@.gp
	${DIFF} expected/$@.gp output/$@.gp

# -selectRange contained in intron on both strands (with -strand)
range_strand_gp_gp: mkout
	${OVERSEL} -selectRange -strand input/rangeSel1.gp input/rangeIn1.gp output/$@.gp
	${DIFF} expected/$@.gp output/$@.gp

# -selectRange contained in intron on both strands (without -strand), non-overlaping
range_nostrand_gp_nonover_gp: mkout
	${OVERSEL} -selectRange -nonOverlapping input/rangeSel1.gp input/rangeIn1.gp output/$@.gp
	${DIFF} expected/$@.gp output/$@.gp

# -selectRange contained in intron on both strands (with -strand), non-overlaping
range_strand_gp_nonover_gp: mkout
	${OVERSEL} -selectRange -strand -nonOverlapping input/rangeSel1.gp input/rangeIn1.gp output/$@.gp
	${DIFF} expected/$@.gp output/$@.gp

# bug were  -selectCds cause idOutput to ignore -excludeSelf
cds_id_out_gp: mkout
	${OVERSEL} -selectCds -idOutput -strand -excludeSelf input/cdsIdBugSelect.gp input/cdsIdBugIn.gp output/$@.ids
	${DIFF} expected/$@.ids output/$@.ids

# 34 column bed causing segv
wideTest: mkout
	${OVERSEL} input/wideSelect.bed input/wideIn.bed stdout > output/$@.bed
	${DIFF} expected/$@.bed output/$@.bed

mkout:
	@${MKDIR} output

clean:
	rm -rf output
