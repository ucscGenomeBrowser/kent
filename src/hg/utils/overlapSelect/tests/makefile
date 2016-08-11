kentSrc = ../../../..
include ../../../../inc/common.mk

overlapSelect = ${DESTBINDIR}/overlapSelect
genePredCheck = ${DESTBINDIR}/genePredCheck
DIFF = diff -u

# To check memory leaks, enable selectTableFree() in overlapSelect.c and
# uncomment overlapSelect redefinition below.
#overlapSelect = valgrind --tool=memcheck --suppressions=../../../lib/valgrind.suppress --num-callers=100 --leak-check=full --leak-resolution=high --show-reachable=yes ${BINDIR}/overlapSelect

test:   pslSelectTests \
	chainSelectTests \
	bedSelectTests \
	excludeSelfTests \
	columnSelectTests \
	cdsOverlapTests \
	mergeTests \
	idOutputTests \
	statsOutputTests \
	statsOutputAllTests \
	statsOutputBothTests \
	thresholdTests \
	similarityTests \
	overlapBasesTests \
	aggregateTests \
	protTests \
	idMatchTests \
	rangeTests \
	cds_id_out_gp \
	wideTest \
	name2EmptyTest \
	orthoStrandTest \
	xenoPslStatsTest \
	xenoPslStatsStrandTest \
	xenoPslGpStatsStrandTest \
	xenoGpPslStatsStrandTest

###
# selecting PSLs
###
pslSelectTests: psl_over_NM_015110gp psl_nonover_NM_015110gp \
		psl_over_NM_015110bed psl_nonover_NM_015110bed psl_over_NM_001206gp \
		psl_nonover_NM_001206gp psl_over_NM_001206bed psl_nonover_NM_001206bed \
		psl_over_NM_015110bed_split psl_nonover_NM_015110bed_split \
		psl_over_psl_stats pslq_over_psl_stats  psl_over_pslq_stats \
		pslq_over_pslq_stats

# select psls overlaping NM_015110 with gp
psl_over_NM_015110gp: mkout
	${overlapSelect} -dropped=output/$@.drop.psl input/NM_015110.gp input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl
	${DIFF} expected/$@.drop.psl output/$@.drop.psl

# select psls overlaping not NM_015110 with gp
psl_nonover_NM_015110gp: mkout
	${overlapSelect} -nonOverlapping -dropped=output/$@.drop.psl input/NM_015110.gp input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl
	${DIFF} expected/$@.drop.psl output/$@.drop.psl

# select psls overlaping NM_015110 with bed
psl_over_NM_015110bed: mkout
	${overlapSelect} input/NM_015110.bed input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl

# select psls overlaping not NM_015110 with bed
psl_nonover_NM_015110bed: mkout
	${overlapSelect} -nonOverlapping input/NM_015110.bed input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl

# select psls overlaping NM_001206 with gp
psl_over_NM_001206gp: mkout
	${overlapSelect} input/NM_001206.gp input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl

# select psls overlaping not NM_001206 with gp
psl_nonover_NM_001206gp: mkout
	${overlapSelect} -nonOverlapping input/NM_001206.gp input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl

# select psls overlaping NM_001206 with bed
psl_over_NM_001206bed: mkout
	${overlapSelect} input/NM_001206.bed input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl

# select psls overlaping not NM_001206 with bed
psl_nonover_NM_001206bed: mkout
	${overlapSelect} -nonOverlapping input/NM_001206.bed input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl

# select psls overlaping NM_015110 with bed, one block per record
psl_over_NM_015110bed_split: mkout
	${overlapSelect} input/NM_015110.split.bed input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl

# select psls overlaping not NM_015110 with bed, one block per record
psl_nonover_NM_015110bed_split: mkout
	${overlapSelect} -nonOverlapping input/NM_015110.split.bed input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl

psl_over_psl_stats: mkout
	${overlapSelect} -statsOutput -excludeSelf input/mrna.psl input/mrna.psl output/$@.stats
	${DIFF} expected/$@.stats output/$@.stats

pslq_over_psl_stats: mkout output/qmrna.psl
	${overlapSelect} -statsOutput -excludeSelf -inFmt=pslq input/mrna.psl output/qmrna.psl output/$@.stats
	${DIFF} expected/$@.stats output/$@.stats

psl_over_pslq_stats: mkout output/qmrna.psl
	${overlapSelect} -statsOutput -excludeSelf -selectFmt=pslq output/qmrna.psl input/mrna.psl output/$@.stats
	${DIFF} expected/$@.stats output/$@.stats

pslq_over_pslq_stats: mkout output/qmrna.psl
	${overlapSelect} -statsOutput -excludeSelf -inFmt=pslq -selectFmt=pslq output/qmrna.psl output/qmrna.psl output/$@.stats
	${DIFF} expected/$@.stats output/$@.stats

output/qmrna.psl: input/mrna.psl mkout
	pslSwap $< $@

###
# chains
###
chainSelectTests: chain_over_NM_015110gp chain_nonover_NM_001206bed \
	chain_over_chain_stats chainq_over_chain_stats chain_over_chainq_stats\
	chainq_over_chainq_stats

# select chains overlaping NM_015110 with gp
chain_over_NM_015110gp: mkout
	${overlapSelect} -dropped=output/$@.drop.chain input/NM_015110.gp input/mrna.chain output/$@.chain
	${DIFF} expected/$@.chain output/$@.chain
	${DIFF} expected/$@.drop.chain output/$@.drop.chain

# select chains overlaping not NM_001206 with bed
chain_nonover_NM_001206bed: mkout
	${overlapSelect} -nonOverlapping input/NM_001206.bed input/mrna.chain output/$@.chain
	${DIFF} expected/$@.chain output/$@.chain

chain_over_chain_stats: mkout
	${overlapSelect} -statsOutput -excludeSelf input/mrna.chain input/mrna.chain output/$@.stats
	${DIFF} expected/$@.stats output/$@.stats

chainq_over_chain_stats: mkout output/qmrna.chain
	${overlapSelect} -statsOutput -excludeSelf -inFmt=chainq input/mrna.chain output/qmrna.chain output/$@.stats
	${DIFF} expected/$@.stats output/$@.stats

chain_over_chainq_stats: mkout output/qmrna.chain
	${overlapSelect} -statsOutput -excludeSelf -selectFmt=chainq output/qmrna.chain input/mrna.chain output/$@.stats
	${DIFF} expected/$@.stats output/$@.stats

chainq_over_chainq_stats: mkout output/qmrna.chain
	${overlapSelect} -statsOutput -excludeSelf -inFmt=chainq -selectFmt=chainq output/qmrna.chain output/qmrna.chain output/$@.stats
	${DIFF} expected/$@.stats output/$@.stats

output/qmrna.chain: input/mrna.chain mkout
	chainSwap $< $@



###
# selecting BEDs
###
bedSelectTests: bed_over_NM_015110gp bed_nonover_NM_015110gp \
		bed_over_NM_015110bed bed_nonover_NM_015110bed  \
		bed_over_NM_015110bed_split bed_nonover_NM_015110bed_split \

# select beds overlaping NM_015110 with gp
bed_over_NM_015110gp: mkout
	${overlapSelect} input/NM_015110.gp -dropped=output/$@.drop.bed input/mrna.bed output/$@.bed
	${DIFF} expected/$@.bed output/$@.bed
	${DIFF} expected/$@.drop.bed output/$@.drop.bed

# select beds overlaping not NM_015110 with gp
bed_nonover_NM_015110gp: mkout
	${overlapSelect} -nonOverlapping -dropped=output/$@.drop.bed  input/NM_015110.gp input/mrna.bed output/$@.bed
	${DIFF} expected/$@.bed output/$@.bed
	${DIFF} expected/$@.drop.bed output/$@.drop.bed

# select beds overlaping NM_015110 with bed
bed_over_NM_015110bed: mkout
	${overlapSelect} input/NM_015110.bed input/mrna.bed output/$@.bed
	${DIFF} expected/$@.bed output/$@.bed

# select beds overlaping not NM_015110 with bed
bed_nonover_NM_015110bed: mkout
	${overlapSelect} -nonOverlapping input/NM_015110.bed input/mrna.bed output/$@.bed
	${DIFF} expected/$@.bed output/$@.bed

# select beds overlaping NM_015110 with bed, one block per record
bed_over_NM_015110bed_split: mkout
	${overlapSelect} input/NM_015110.split.bed input/mrna.bed output/$@.bed
	${DIFF} expected/$@.bed output/$@.bed

bed_nonover_NM_015110bed_split: mkout
	${overlapSelect} -nonOverlapping input/NM_015110.split.bed input/mrna.bed output/$@.bed
	${DIFF} expected/$@.bed output/$@.bed

###
# -excludeSelf tests
##
excludeSelfTests: exludeself_over_mrna1 exludeself_nonover_mrna1 \
		exludeself_over_mrna2 exludeself_nonover_mrna2

# Test -excludeSelf option with file containing two non-overlaping mRNAs.
# Neither should be selected.
exludeself_over_mrna1: mkout
	${overlapSelect} -excludeSelf input/mrna-self1.psl input/mrna-self1.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl

# Test -excludeSelf and -nonOverlapping options with file containing two
# non-overlaping mRNAs. Both should be selected.
exludeself_nonover_mrna1: mkout
	${overlapSelect} -excludeSelf -nonOverlapping input/mrna-self1.psl input/mrna-self1.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl

# Test of -excludeSelf with file containing 3 overlap mRNAs and 1
# non-overlaping.  Overlaping should be selected, since they are overlapped by
# more than self.
exludeself_over_mrna2: mkout
	${overlapSelect} -excludeSelf input/mrna-self2.psl input/mrna-self2.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl

# Test of -excludeSelf and -nonOverlapping with file containing 3 overlap mRNAs
# and 1 non-overlaping.  Only non-overlaping should be selected.
exludeself_nonover_mrna2: mkout
	${overlapSelect} -excludeSelf -nonOverlapping input/mrna-self2.psl input/mrna-self2.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl

###
# select by column tests
###
columnSelectTests: colSelect colExclude colSelectStrand colStats

# select by column
colSelect: mkout
	${overlapSelect} -selectCoordCols=1,2,3 -inCoordCols=3,4,5 \
	    input/gene.select input/gene.clusters output/$@.cluster
	${DIFF} expected/$@.cluster output/$@.cluster

# exclude by column
colExclude: mkout
	${overlapSelect} -nonOverlapping -selectCoordCols=1,2,3 -inCoordCols=3,4,5 \
	    input/gene.select input/gene.clusters output/$@.cluster
	${DIFF} expected/$@.cluster output/$@.cluster

# select by column, including strand
colSelectStrand: mkout
	${overlapSelect} -strand -selectCoordCols=1,2,3,4 -inCoordCols=3,4,5,6 \
	    input/gene.select input/gene.clusters output/$@.cluster
	${DIFF} expected/$@.cluster output/$@.cluster

# stats output, by column, including name, ignoring strand
colStats: mkout
	${overlapSelect} -statsOutput -selectCoordCols=1,2,3,,0 -inCoordCols=3,4,5,,2 \
	    input/gene.select input/gene.clusters output/$@.cluster
	${DIFF} expected/$@.cluster output/$@.cluster

###
# CDS overlap tests
###
cdsOverlapTests: cdsOverlapCdsCds cdsOverlapCdsExon \
		cdsOverlapExonCds noCdsSelect noCdsSelectNoOver

# CDS doesn't overlap
cdsOverlapCdsCds: mkout
	${overlapSelect} -inCds -selectCds input/cdsOverlap1.gp input/cdsOverlap2.gp output/$@.gp
	${DIFF} expected/$@.gp output/$@.gp

cdsOverlapCdsExon: mkout
	${overlapSelect} -inCds input/cdsOverlap1.gp input/cdsOverlap2.gp output/$@.gp
	${DIFF} expected/$@.gp output/$@.gp

cdsOverlapExonCds: mkout
	${overlapSelect} -selectCds input/cdsOverlap1.gp input/cdsOverlap2.gp output/$@.gp
	${DIFF} expected/$@.gp output/$@.gp

# regression: select with CDS and a genePred with no CDS
noCdsSelect: mkout
	${overlapSelect} -selectCds input/noCds.gp input/noCds2.gp output/$@.gp
	${DIFF} expected/$@.gp output/$@.gp
noCdsSelectNoOver: mkout
	${overlapSelect} -nonOverlapping -selectCds input/noCds.gp input/noCds2.gp output/$@.gp
	${DIFF} expected/$@.gp output/$@.gp

###
# -merge tests
###
mergeTests: psl_over_NM_015110gpMerge psl_over_NM_001206bedMerge \
	bed_over_NM_015110bedMerge colSelectMerge

# select psls overlaping NM_015110 with gp and merge
psl_over_NM_015110gpMerge: mkout
	${overlapSelect} -mergeOutput -dropped=output/$@.drop.psl input/NM_015110.gp input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl
	${DIFF} expected/$@.drop.psl output/$@.drop.psl

# select psls overlaping NM_001206 with bed and merge
psl_over_NM_001206bedMerge: mkout
	${overlapSelect} -mergeOutput input/NM_001206.bed input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl

# select beds overlaping NM_015110 with bed and merge
bed_over_NM_015110bedMerge: mkout
	${overlapSelect} -mergeOutput input/NM_015110.bed input/mrna.bed output/$@.bed
	${DIFF} expected/$@.bed output/$@.bed

# select by column and merge
colSelectMerge: mkout
	${overlapSelect} -mergeOutput -selectCoordCols=1,2,3 -inCoordCols=3,4,5 \
	    input/gene.select input/gene.clusters output/$@.cluster
	${DIFF} expected/$@.cluster output/$@.cluster

###
# -idOutput tests
###
idOutputTests: psl_over_NM_015110gpIdOutput psl_over_NM_001206bedIdOutput \
	bed_over_NM_015110bedIdOutput

# select psls overlaping NM_015110 with gp and idOutput
psl_over_NM_015110gpIdOutput: mkout
	${overlapSelect} -idOutput -dropped=output/$@.drop.ids input/NM_015110.gp input/mrna.psl output/$@.ids
	${DIFF} expected/$@.ids output/$@.ids
	${DIFF} expected/$@.drop.ids output/$@.drop.ids

# select psls overlaping NM_001206 with bed and idOutput
psl_over_NM_001206bedIdOutput: mkout
	${overlapSelect} -idOutput input/NM_001206.bed input/mrna.psl output/$@.ids
	${DIFF} expected/$@.ids output/$@.ids

# select beds overlaping NM_015110 with bed and idOutput
bed_over_NM_015110bedIdOutput: mkout
	${overlapSelect} -idOutput input/NM_015110.bed input/mrna.bed output/$@.ids
	${DIFF} expected/$@.ids output/$@.ids

###
# -statsOutput tests
###
statsOutputTests: psl_over_NM_015110gpStatsOutput psl_over_NM_001206bedStatsOutput \
	bed_over_NM_015110bedStatsOutput

# select psls overlaping NM_015110 with gp and statsOutput
psl_over_NM_015110gpStatsOutput: mkout
	${overlapSelect} -statsOutput input/NM_015110.gp input/mrna.psl output/$@.stats
	${DIFF} expected/$@.stats output/$@.stats

# select psls overlaping NM_001206 with bed and statsOutput
psl_over_NM_001206bedStatsOutput: mkout
	${overlapSelect} -statsOutput input/NM_001206.bed input/mrna.psl output/$@.stats
	${DIFF} expected/$@.stats output/$@.stats

# select beds overlaping NM_015110 with bed and statsOutput
bed_over_NM_015110bedStatsOutput: mkout
	${overlapSelect} -statsOutput input/NM_015110.bed input/mrna.bed output/$@.stats
	${DIFF} expected/$@.stats output/$@.stats

###
# -statsOutputAll tests
###
statsOutputAllTests: psl_over_NM_015110gpStatsOutputAll psl_over_NM_001206bedStatsOutputAll \
	bed_over_NM_015110bedStatsOutputAll

# select psls overlaping NM_015110 with gp and statsOutputAll
psl_over_NM_015110gpStatsOutputAll: mkout
	${overlapSelect} -statsOutputAll input/NM_015110.gp input/mrna.psl output/$@.stats
	${DIFF} expected/$@.stats output/$@.stats

# select psls overlaping NM_001206 with bed and statsOutputAll
psl_over_NM_001206bedStatsOutputAll: mkout
	${overlapSelect} -statsOutputAll input/NM_001206.bed input/mrna.psl output/$@.stats
	${DIFF} expected/$@.stats output/$@.stats

# select beds overlaping NM_015110 with bed and statsOutputAll
bed_over_NM_015110bedStatsOutputAll: mkout
	${overlapSelect} -statsOutputAll input/NM_015110.bed input/mrna.bed output/$@.stats
	${DIFF} expected/$@.stats output/$@.stats

###
# -statsOutputBoth tests
###
statsOutputBothTests: psl_over_bedStatsOutputBoth bed_over_bedStatsOutputBoth

# select psls overlaping bed and statsOutputBoth
psl_over_bedStatsOutputBoth: mkout
	${overlapSelect} -statsOutputBoth input/statsSelect.bed input/mrna.psl output/$@.stats
	${DIFF} expected/$@.stats output/$@.stats

# select beds overlaping with bed and statsOutputBoth
bed_over_bedStatsOutputBoth: mkout
	${overlapSelect} -statsOutputBoth input/statsSelect.bed input/mrna.bed output/$@.stats
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
	${overlapSelect} -overlapThreshold=0.4 -dropped=output/$@.drop.psl input/NM_015110.gp input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl
	${DIFF} expected/$@.drop.psl output/$@.drop.psl

# select psls overlaping not NM_015110 with gp and overlapThreshold
psl_gp_non_threshold: mkout
	${overlapSelect} -overlapThreshold=0.4 -nonOverlapping -dropped=output/$@.drop.psl input/NM_015110.gp input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl
	${DIFF} expected/$@.drop.psl output/$@.drop.psl

# 95% threshold
gp_gp_threshold95: mkout
	${overlapSelect} -idOutput -overlapThreshold=0.95 input/NM_015110.gp  input/NM_015110Similar.gp output/$@.ids
	${overlapSelect} -idOutput -overlapThreshold=0.95 input/NM_015110Similar.gp input/NM_015110.gp output/$@Rev.ids
	${DIFF} expected/$@.ids output/$@.ids
	${DIFF} expected/$@Rev.ids output/$@Rev.ids

# 75% threshold
gp_gp_threshold75: mkout
	${overlapSelect} -idOutput -overlapThreshold=0.75 input/NM_015110.gp  input/NM_015110Similar.gp output/$@.ids
	${overlapSelect} -idOutput -overlapThreshold=0.75 input/NM_015110Similar.gp input/NM_015110.gp output/$@Rev.ids
	${DIFF} expected/$@.ids output/$@.ids
	${DIFF} expected/$@Rev.ids output/$@Rev.ids

# 100% threshold
gp_gp_threshold100: mkout
	${overlapSelect} -idOutput -overlapThreshold=1.0 input/NM_015110.gp  input/NM_015110Similar.gp output/$@.ids
	${overlapSelect} -idOutput -overlapThreshold=1.0 input/NM_015110Similar.gp input/NM_015110.gp output/$@Rev.ids
	${DIFF} expected/$@.ids output/$@.ids
	${DIFF} expected/$@Rev.ids output/$@Rev.ids

# 95% CDS threshold
gp_gp_thresholdCds95: mkout
	${overlapSelect} -idOutput -overlapThreshold=0.95 -selectCds -inCds input/NM_015110.gp  input/NM_015110Similar.gp output/$@.ids
	${overlapSelect} -idOutput -overlapThreshold=0.95 -selectCds -inCds input/NM_015110Similar.gp input/NM_015110.gp output/$@Rev.ids
	${DIFF} expected/$@.ids output/$@.ids
	${DIFF} expected/$@Rev.ids output/$@Rev.ids

# 75% CDS threshold
gp_gp_thresholdCds75: mkout
	${overlapSelect} -idOutput -overlapThreshold=0.75 -selectCds -inCds input/NM_015110.gp  input/NM_015110Similar.gp output/$@.ids
	${overlapSelect} -idOutput -overlapThreshold=0.75 -selectCds -inCds input/NM_015110Similar.gp input/NM_015110.gp output/$@Rev.ids
	${DIFF} expected/$@.ids output/$@.ids
	${DIFF} expected/$@Rev.ids output/$@Rev.ids

# 100% CDS threshold
gp_gp_thresholdCds100: mkout
	${overlapSelect} -idOutput -overlapThreshold=1.0 -selectCds -inCds input/NM_015110.gp  input/NM_015110Similar.gp output/$@.ids
	${overlapSelect} -idOutput -overlapThreshold=1.0 -selectCds -inCds input/NM_015110Similar.gp input/NM_015110.gp output/$@Rev.ids
	${DIFF} expected/$@.ids output/$@.ids
	${DIFF} expected/$@Rev.ids output/$@Rev.ids

# select psls overlaping NM_015110 with gp, overlapThreshold, overlapThresholdCeil
psl_gp_threshold_ceil: mkout
	${overlapSelect} -overlapThreshold=0.4 -overlapThresholdCeil=0.6 -dropped=output/$@.drop.psl input/NM_015110.gp input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl
	${DIFF} expected/$@.drop.psl output/$@.drop.psl

###
# similarity tests
###
similarityTests: psl_gp_similarity  psl_gp_non_similarity \
	gp_gp_similarity95 gp_gp_similarity75 gp_gp_similarity100 \
	gp_gp_similarityCds95 gp_gp_similarityCds75 gp_gp_similarityCds100 \
	psl_gp_similarity_ceil bed_threshold_ceil_only bed_threshold_ceil_only_no

# select psls overlaping NM_015110 with gp and overlapSimilarity
psl_gp_similarity: mkout
	${overlapSelect} -overlapSimilarity=0.4 -dropped=output/$@.drop.psl input/NM_015110.gp input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl
	${DIFF} expected/$@.drop.psl output/$@.drop.psl

# select psls overlaping not NM_015110 with gp and overlapSimilarity
psl_gp_non_similarity: mkout
	${overlapSelect} -overlapSimilarity=0.4 -nonOverlapping -dropped=output/$@.drop.psl input/NM_015110.gp input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl
	${DIFF} expected/$@.drop.psl output/$@.drop.psl

# 95% similarity
gp_gp_similarity95: mkout
	${overlapSelect} -idOutput -overlapSimilarity=0.95 input/NM_015110.gp  input/NM_015110Similar.gp output/$@.ids
	${overlapSelect} -idOutput -overlapSimilarity=0.95 input/NM_015110Similar.gp input/NM_015110.gp output/$@Rev.ids
	${DIFF} expected/$@.ids output/$@.ids
	${DIFF} expected/$@Rev.ids output/$@Rev.ids

# 75% similarity
gp_gp_similarity75: mkout
	${overlapSelect} -idOutput -overlapSimilarity=0.75 input/NM_015110.gp  input/NM_015110Similar.gp output/$@.ids
	${overlapSelect} -idOutput -overlapSimilarity=0.75 input/NM_015110Similar.gp input/NM_015110.gp output/$@Rev.ids
	${DIFF} expected/$@.ids output/$@.ids
	${DIFF} expected/$@Rev.ids output/$@Rev.ids

# 100% similarity
gp_gp_similarity100: mkout
	${overlapSelect} -idOutput -overlapSimilarity=1.0 input/NM_015110.gp  input/NM_015110Similar.gp output/$@.ids
	${overlapSelect} -idOutput -overlapSimilarity=1.0 input/NM_015110Similar.gp input/NM_015110.gp output/$@Rev.ids
	${DIFF} expected/$@.ids output/$@.ids
	${DIFF} expected/$@Rev.ids output/$@Rev.ids

# 95% CDS similarity
gp_gp_similarityCds95: mkout
	${overlapSelect} -idOutput -overlapSimilarity=0.95 -selectCds -inCds input/NM_015110.gp  input/NM_015110Similar.gp output/$@.ids
	${overlapSelect} -idOutput -overlapSimilarity=0.95 -selectCds -inCds input/NM_015110Similar.gp input/NM_015110.gp output/$@Rev.ids
	${DIFF} expected/$@.ids output/$@.ids
	${DIFF} expected/$@Rev.ids output/$@Rev.ids

# 75% CDS similarity
gp_gp_similarityCds75: mkout
	${overlapSelect} -idOutput -overlapSimilarity=0.75 -selectCds -inCds input/NM_015110.gp  input/NM_015110Similar.gp output/$@.ids
	${overlapSelect} -idOutput -overlapSimilarity=0.75 -selectCds -inCds input/NM_015110Similar.gp input/NM_015110.gp output/$@Rev.ids
	${DIFF} expected/$@.ids output/$@.ids
	${DIFF} expected/$@Rev.ids output/$@Rev.ids

# 100% CDS similarity
gp_gp_similarityCds100: mkout
	${overlapSelect} -idOutput -overlapSimilarity=1.0 -selectCds -inCds input/NM_015110.gp  input/NM_015110Similar.gp output/$@.ids
	${overlapSelect} -idOutput -overlapSimilarity=1.0 -selectCds -inCds input/NM_015110Similar.gp input/NM_015110.gp output/$@Rev.ids
	${DIFF} expected/$@.ids output/$@.ids
	${DIFF} expected/$@Rev.ids output/$@Rev.ids

# select psls overlaping NM_015110 with gp, overlapSimilarity, and  overlapSimilarityCeil
psl_gp_similarity_ceil: mkout
	${overlapSelect} -overlapSimilarity=0.4 -overlapSimilarityCeil=0.75 -dropped=output/$@.drop.psl input/NM_015110.gp input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl
	${DIFF} expected/$@.drop.psl output/$@.drop.psl

# only threshold ceiling
bed_threshold_ceil_only: mkout
	${overlapSelect} -overlapThresholdCeil=0.2 input/ceil1.sel.bed input/ceil1.in.bed output/$@.bed
	${DIFF} expected/$@.bed output/$@.bed
bed_threshold_ceil_only_no: mkout
	${overlapSelect} -overlapThresholdCeil=0.01 input/ceil1.sel.bed input/ceil1.in.bed output/$@.bed
	${DIFF} expected/$@.bed output/$@.bed


###
# -overlapBases tests
###
overlapBasesTests: overlapBases250 overlapBases2100 overlapBases3000

overlapBases250: mkout
	${overlapSelect} -overlapBases=250 input/NM_015110.bed input/mrna.bed output/$@.bed
	${DIFF} expected/$@.bed output/$@.bed

overlapBases2100: mkout
	${overlapSelect} -overlapBases=2100 input/NM_015110.bed input/mrna.bed output/$@.bed
	${DIFF} expected/$@.bed output/$@.bed

overlapBases3000: mkout
	${overlapSelect} -overlapBases=3000 input/NM_015110.bed input/mrna.bed output/$@.bed
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
	${overlapSelect} -aggregate -overlapThreshold=0.4 -dropped=output/$@.drop.psl input/NM_015110.gp input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl
	${DIFF} expected/$@.drop.psl output/$@.drop.psl

# select psls overlaping not NM_015110 with gp and overlapThreshold
psl_gp_non_aggregate: mkout
	${overlapSelect} -aggregate -overlapThreshold=0.4 -nonOverlapping -dropped=output/$@.drop.psl input/NM_015110.gp input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl
	${DIFF} expected/$@.drop.psl output/$@.drop.psl

# 95% aggregate
gp_gp_aggregate95: mkout
	${overlapSelect} -aggregate -idOutput -overlapThreshold=0.95 input/NM_015110.gp  input/NM_015110Similar.gp output/$@.ids
	${overlapSelect} -aggregate -idOutput -overlapThreshold=0.95 input/NM_015110Similar.gp input/NM_015110.gp output/$@Rev.ids
	${DIFF} expected/$@.ids output/$@.ids
	${DIFF} expected/$@Rev.ids output/$@Rev.ids

# 75% aggregate
gp_gp_aggregate75: mkout
	${overlapSelect} -aggregate -idOutput -overlapThreshold=0.75 input/NM_015110.gp  input/NM_015110Similar.gp output/$@.ids
	${overlapSelect} -aggregate -idOutput -overlapThreshold=0.75 input/NM_015110Similar.gp input/NM_015110.gp output/$@Rev.ids
	${DIFF} expected/$@.ids output/$@.ids
	${DIFF} expected/$@Rev.ids output/$@Rev.ids

# 100% aggregate
gp_gp_aggregate100: mkout
	${overlapSelect} -aggregate -idOutput -overlapThreshold=1.0 input/NM_015110.gp  input/NM_015110Similar.gp output/$@.ids
	${overlapSelect} -aggregate -idOutput -overlapThreshold=1.0 input/NM_015110Similar.gp input/NM_015110.gp output/$@Rev.ids
	${DIFF} expected/$@.ids output/$@.ids
	${DIFF} expected/$@Rev.ids output/$@Rev.ids

# 95% CDS aggregate
gp_gp_aggregateCds95: mkout
	${overlapSelect} -aggregate -idOutput -overlapThreshold=0.95 -selectCds -inCds input/NM_015110.gp  input/NM_015110Similar.gp output/$@.ids
	${overlapSelect} -aggregate -idOutput -overlapThreshold=0.95 -selectCds -inCds input/NM_015110Similar.gp input/NM_015110.gp output/$@Rev.ids
	${DIFF} expected/$@.ids output/$@.ids
	${DIFF} expected/$@Rev.ids output/$@Rev.ids

# 75% CDS aggregate
gp_gp_aggregateCds75: mkout
	${overlapSelect} -aggregate -idOutput -overlapThreshold=0.75 -selectCds -inCds input/NM_015110.gp  input/NM_015110Similar.gp output/$@.ids
	${overlapSelect} -aggregate -idOutput -overlapThreshold=0.75 -selectCds -inCds input/NM_015110Similar.gp input/NM_015110.gp output/$@Rev.ids
	${DIFF} expected/$@.ids output/$@.ids
	${DIFF} expected/$@Rev.ids output/$@Rev.ids

# 100% CDS aggregate
gp_gp_aggregateCds100: mkout
	${overlapSelect} -aggregate -idOutput -overlapThreshold=1.0 -selectCds -inCds input/NM_015110.gp  input/NM_015110Similar.gp output/$@.ids
	${overlapSelect} -aggregate -idOutput -overlapThreshold=1.0 -selectCds -inCds input/NM_015110Similar.gp input/NM_015110.gp output/$@Rev.ids
	${DIFF} expected/$@.ids output/$@.ids
	${DIFF} expected/$@Rev.ids output/$@Rev.ids

# aggregate stats
gp_gp_aggregateStats: mkout
	${overlapSelect} -aggregate -statsOutput input/NM_015110.gp  input/NM_015110Similar.gp output/$@.stats
	${DIFF} expected/$@.stats output/$@.stats

# aggregate stats on all records
psl_gp_aggregateStatsAll: mkout
	${overlapSelect} -aggregate -statsOutputAll input/NM_015110.gp input/mrna.psl output/$@.stats
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
	${overlapSelect} -strand -overlapSimilarity=0.9 -selectCds input/refGeneForProt.gp  input/prot.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl

gp_psl_protStats: mkout
	${overlapSelect} -statsOutput -strand -selectCds input/refGeneForProt.gp  input/prot.psl output/$@.stats
	${DIFF} expected/$@.stats output/$@.stats

psl_gp_prot80: mkout
	${overlapSelect} -strand -overlapSimilarity=0.9 -inCds input/prot.psl input/refGeneForProt.gp  output/$@.gp
	${DIFF} expected/$@.gp output/$@.gp

psl_gp_protStats: mkout
	${overlapSelect} -statsOutput -strand -inCds input/prot.psl input/refGeneForProt.gp output/$@.stats
	${DIFF} expected/$@.stats output/$@.stats

###
# idMatch tests
###
idMatchTests: psl_idMatch psl_idMatch_non idMatchSimple idMatchSimpleNon

psl_idMatch: mkout
	${overlapSelect} -idMatch -overlapSimilarity=0.8 -dropped=output/$@.drop.psl input/mrna-idMatch1.psl input/mrna-idMatch2.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl
	${DIFF} expected/$@.drop.psl output/$@.drop.psl

psl_idMatch_non: mkout
	${overlapSelect} -idMatch -overlapSimilarity=0.8 -nonOverlapping -dropped=output/$@.drop.psl input/mrna-idMatch1.psl input/mrna-idMatch2.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl
	${DIFF} expected/$@.drop.psl output/$@.drop.psl

idMatchSimple: mkout
	${overlapSelect} -idMatch input/idMatchSimple1.gp input/idMatchSimple2.gp output/$@.gp
	${DIFF} expected/$@.gp output/$@.gp

idMatchSimpleNon: mkout
	${overlapSelect} -idMatch -nonOverlapping input/idMatchSimple1.gp input/idMatchSimple2.gp output/$@.gp
	${DIFF} expected/$@.gp output/$@.gp


###
# whole range overlap
###
rangeTests: range_psl_over_NM_015110gp range_psl_nonover_NM_015110gp \
	    range_psl_over_NM_015110bed range_psl_nonover_NM_015110bed \
	    range_NM_015110gp_over_psl range_NM_015110gp_nonover_psl \
	    range_NM_015110bed_over_psl range_NM_015110bed_nonover_psl \
	    range_nostrand_gp_gp range_strand_gp_gp range_oppositeStrand_gp_gp \
	    range_nostrand_gp_nonover_gp range_strand_gp_nonover_gp

# range select psls overlaping NM_015110 with gp
range_psl_over_NM_015110gp: mkout
	${overlapSelect} -selectRange -dropped=output/$@.drop.psl input/NM_015110.gp input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl
	${DIFF} expected/$@.drop.psl output/$@.drop.psl

# range select psls overlaping not NM_015110 with gp
range_psl_nonover_NM_015110gp: mkout
	${overlapSelect} -selectRange -nonOverlapping -dropped=output/$@.drop.psl input/NM_015110.gp input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl
	${DIFF} expected/$@.drop.psl output/$@.drop.psl

# range select psls overlaping NM_015110 with bed
range_psl_over_NM_015110bed: mkout
	${overlapSelect} -selectRange input/NM_015110.bed input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl

# range select psls not overlaping NM_015110 with bed
range_psl_nonover_NM_015110bed: mkout
	${overlapSelect} -selectRange -nonOverlapping input/NM_015110.bed input/mrna.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl

# range select genePred overlapping psls
range_NM_015110gp_over_psl: mkout
	${overlapSelect} -inRange input/inNM_015110.psl input/NM_015110.gp output/$@.gp
	${DIFF} expected/$@.gp output/$@.gp

# range select genePred not overlapping psls
range_NM_015110gp_nonover_psl: mkout
	${overlapSelect} -inRange -nonOverlapping input/inNM_015110.psl input/NM_015110.gp output/$@.gp
	${DIFF} expected/$@.gp output/$@.gp

# range select bed overlaping psls
range_NM_015110bed_over_psl: mkout
	${overlapSelect} -inRange input/inNM_015110.psl input/NM_015110.bed output/$@.bed
	${DIFF} expected/$@.bed output/$@.bed

# range select psls overlaping not NM_015110 with bed
range_NM_015110bed_nonover_psl: mkout
	${overlapSelect} -inRange -nonOverlapping input/inNM_015110.psl input/NM_015110.bed output/$@.bed
	${DIFF} expected/$@.bed output/$@.bed

# -selectRange contained in intron on both strands (without -strand)
range_nostrand_gp_gp: mkout
	${overlapSelect} -selectRange input/rangeSel1.gp input/rangeIn1.gp output/$@.gp
	${DIFF} expected/$@.gp output/$@.gp

# -selectRange contained in intron on both strands (with -strand)
range_strand_gp_gp: mkout
	${overlapSelect} -selectRange -strand input/rangeSel1.gp input/rangeIn1.gp output/$@.gp
	${DIFF} expected/$@.gp output/$@.gp

# -selectRange contained in intron on both strands (with -oppositeStrand)
range_oppositeStrand_gp_gp: mkout
	${overlapSelect} -selectRange -oppositeStrand input/rangeSel1.gp input/rangeIn1.gp output/$@.gp
	${DIFF} expected/$@.gp output/$@.gp

# -selectRange contained in intron on both strands (without -strand), non-overlaping
range_nostrand_gp_nonover_gp: mkout
	${overlapSelect} -selectRange -nonOverlapping input/rangeSel1.gp input/rangeIn1.gp output/$@.gp
	${DIFF} expected/$@.gp output/$@.gp

# -selectRange contained in intron on both strands (with -strand), non-overlaping
range_strand_gp_nonover_gp: mkout
	${overlapSelect} -selectRange -strand -nonOverlapping input/rangeSel1.gp input/rangeIn1.gp output/$@.gp
	${DIFF} expected/$@.gp output/$@.gp

# bug were  -selectCds cause idOutput to ignore -excludeSelf
cds_id_out_gp: mkout
	${overlapSelect} -selectCds -idOutput -strand -excludeSelf input/cdsIdBugSelect.gp input/cdsIdBugIn.gp output/$@.ids
	${DIFF} expected/$@.ids output/$@.ids

# 34 column bed causing segv
wideTest: mkout
	${overlapSelect} input/wideSelect.bed input/wideIn.bed stdout > output/$@.bed
	${DIFF} expected/$@.bed output/$@.bed

# genePredExt with empty name caused grief
name2EmptyTest: mkout
	${overlapSelect} input/name2Empty.gp input/name2Empty.gp output/$@.gp
	${genePredCheck} -verbose=0 output/$@.gp
	${DIFF} expected/$@.gp output/$@.gp

# psl strand regression
orthoStrandTest: mkout
	${overlapSelect} -strand -aggregate -statsOutputAll input/orthoSel.psl input/orthoIn.gp output/$@.stats
	${DIFF} expected/$@.stats output/$@.stats

# handle of translated alignments is not straight foward
# -+ == - strand
# +- == - strand
# -- == + strand
xenoPslStatsTest: mkout
	${overlapSelect} -statsOutputAll -excludeSelf input/xenoEst1.psl input/xenoEst1.psl output/$@.stats
	${DIFF} expected/$@.stats output/$@.stats

xenoPslStatsStrandTest: mkout
	${overlapSelect} -strand -statsOutputAll -excludeSelf input/xenoEst1.psl input/xenoEst1.psl output/$@.stats
	${DIFF} expected/$@.stats output/$@.stats

xenoPslGpStatsStrandTest: mkout
	${overlapSelect} -strand -statsOutputAll -excludeSelf input/xenoEst1.psl input/xenoEst1.gp output/$@.stats
	${DIFF} expected/$@.stats output/$@.stats

xenoGpPslStatsStrandTest: mkout
	${overlapSelect} -strand -statsOutputAll -excludeSelf input/xenoEst1.gp input/xenoEst1.psl output/$@.stats
	${DIFF} expected/$@.stats output/$@.stats

mkout:
	@${MKDIR} output

clean:
	rm -rf output
