include ../../../inc/common.mk

OVERSEL = ../overlapSelect
DIFF = diff -u


test:   psl_over_NM_015110gp psl_nonover_NM_015110gp \
	psl_over_NM_015110bed psl_nonover_NM_015110bed \
	psl_over_NM_001206gp psl_nonover_NM_001206gp \
	psl_over_NM_001206bed psl_nonover_NM_001206bed \
	psl_over_NM_015110bed_split psl_nonover_NM_015110bed_split \
	bed_over_NM_015110gp bed_nonover_NM_015110gp \
	bed_over_NM_015110bed bed_nonover_NM_015110bed  \
	bed_over_NM_015110bed_split bed_nonover_NM_015110bed_split \
	exludeself_over_mrna1 exludeself_nonover_mrna1 \
	exludeself_over_mrna2 exludeself_nonover_mrna2 \
	colSelect colExclude cdsOverlapCdsCds cdsOverlapCdsExon \
	cdsOverlapExonCds

# select psls overlaping NM_015110 with gp
psl_over_NM_015110gp: mkout
	${OVERSEL} -dropped=output/psl_over_NM_015110gp.drop.psl input/NM_015110.gp input/mrna.psl output/psl_over_NM_015110gp.psl
	${DIFF} expected/psl_over_NM_015110gp.psl output/psl_over_NM_015110gp.psl
	${DIFF} expected/psl_over_NM_015110gp.drop.psl output/psl_over_NM_015110gp.drop.psl

# select psls overlaping not NM_015110 with gp
psl_nonover_NM_015110gp: mkout
	${OVERSEL} -nonOverlaping -dropped=output/psl_nonover_NM_015110gp.drop.psl input/NM_015110.gp input/mrna.psl output/psl_nonover_NM_015110gp.psl
	${DIFF} expected/psl_nonover_NM_015110gp.psl output/psl_nonover_NM_015110gp.psl
	${DIFF} expected/psl_nonover_NM_015110gp.drop.psl output/psl_nonover_NM_015110gp.drop.psl

# select psls overlaping NM_015110 with bed
psl_over_NM_015110bed: mkout
	${OVERSEL} input/NM_015110.bed input/mrna.psl output/psl_over_NM_015110bed.psl
	${DIFF} expected/psl_over_NM_015110bed.psl output/psl_over_NM_015110bed.psl

# select psls overlaping not NM_015110 with bed
psl_nonover_NM_015110bed: mkout
	${OVERSEL} -nonOverlaping input/NM_015110.bed input/mrna.psl output/psl_nonover_NM_015110bed.psl
	${DIFF} expected/psl_nonover_NM_015110bed.psl output/psl_nonover_NM_015110bed.psl

# select psls overlaping NM_001206 with gp
psl_over_NM_001206gp: mkout
	${OVERSEL} input/NM_001206.gp input/mrna.psl output/psl_over_NM_001206gp.psl
	${DIFF} expected/psl_over_NM_001206gp.psl output/psl_over_NM_001206gp.psl

# select psls overlaping not NM_001206 with gp
psl_nonover_NM_001206gp: mkout
	${OVERSEL} -nonOverlaping input/NM_001206.gp input/mrna.psl output/psl_nonover_NM_001206gp.psl
	${DIFF} expected/psl_nonover_NM_001206gp.psl output/psl_nonover_NM_001206gp.psl

# select psls overlaping NM_001206 with bed
psl_over_NM_001206bed: mkout
	${OVERSEL} input/NM_001206.bed input/mrna.psl output/psl_over_NM_001206bed.psl
	${DIFF} expected/psl_over_NM_001206bed.psl output/psl_over_NM_001206bed.psl

# select psls overlaping not NM_001206 with bed
psl_nonover_NM_001206bed: mkout
	${OVERSEL} -nonOverlaping input/NM_001206.bed input/mrna.psl output/psl_nonover_NM_001206bed.psl
	${DIFF} expected/psl_nonover_NM_001206bed.psl output/psl_nonover_NM_001206bed.psl

# select psls overlaping NM_015110 with bed, one block per record
psl_over_NM_015110bed_split: mkout
	${OVERSEL} input/NM_015110.split.bed input/mrna.psl output/psl_over_NM_015110bed_split.psl
	${DIFF} expected/psl_over_NM_015110bed_split.psl output/psl_over_NM_015110bed_split.psl

# select psls overlaping not NM_015110 with bed, one block per record
psl_nonover_NM_015110bed_split: mkout
	${OVERSEL} -nonOverlaping input/NM_015110.split.bed input/mrna.psl output/psl_nonover_NM_015110bed_split.psl
	${DIFF} expected/psl_nonover_NM_015110bed_split.psl output/psl_nonover_NM_015110bed_split.psl

# select beds overlaping NM_015110 with gp
bed_over_NM_015110gp: mkout
	${OVERSEL} input/NM_015110.gp -dropped=output/bed_over_NM_015110gp.drop.bed input/mrna.bed output/bed_over_NM_015110gp.bed
	${DIFF} expected/bed_over_NM_015110gp.bed output/bed_over_NM_015110gp.bed
	${DIFF} expected/bed_over_NM_015110gp.drop.bed output/bed_over_NM_015110gp.drop.bed

# select beds overlaping not NM_015110 with gp
bed_nonover_NM_015110gp: mkout
	${OVERSEL} -nonOverlaping -dropped=output/bed_nonover_NM_015110gp.drop.bed  input/NM_015110.gp input/mrna.bed output/bed_nonover_NM_015110gp.bed
	${DIFF} expected/bed_nonover_NM_015110gp.bed output/bed_nonover_NM_015110gp.bed
	${DIFF} expected/bed_nonover_NM_015110gp.drop.bed output/bed_nonover_NM_015110gp.drop.bed

# select beds overlaping NM_015110 with bed
bed_over_NM_015110bed: mkout
	${OVERSEL} input/NM_015110.bed input/mrna.bed output/bed_over_NM_015110bed.bed
	${DIFF} expected/bed_over_NM_015110bed.bed output/bed_over_NM_015110bed.bed

# select beds overlaping not NM_015110 with bed
bed_nonover_NM_015110bed: mkout
	${OVERSEL} -nonOverlaping input/NM_015110.bed input/mrna.bed output/bed_nonover_NM_015110bed.bed
	${DIFF} expected/bed_nonover_NM_015110bed.bed output/bed_nonover_NM_015110bed.bed

# select beds overlaping NM_015110 with bed, one block per record
bed_over_NM_015110bed_split: mkout
	${OVERSEL} input/NM_015110.split.bed input/mrna.bed output/bed_over_NM_015110bed_split.bed
	${DIFF} expected/bed_over_NM_015110bed.bed output/bed_over_NM_015110bed.bed

bed_nonover_NM_015110bed_split: mkout
	${OVERSEL} -nonOverlaping input/NM_015110.split.bed input/mrna.bed output/bed_nonover_NM_015110bed_split.bed
	${DIFF} expected/bed_nonover_NM_015110bed_split.bed output/bed_nonover_NM_015110bed_split.bed

# Test -excludeSelf option with file containing two non-overlaping mRNAs.
# Neither should be selected.
exludeself_over_mrna1: mkout
	${OVERSEL} -excludeSelf input/mrna-self1.psl input/mrna-self1.psl output/exludeself_over_mrna1.psl
	${DIFF} expected/exludeself_over_mrna1.psl output/exludeself_over_mrna1.psl

# Test -excludeSelf and -nonOverlaping options with file containing two
# non-overlaping mRNAs. Both should be selected.
exludeself_nonover_mrna1: mkout
	${OVERSEL} -excludeSelf -nonOverlaping input/mrna-self1.psl input/mrna-self1.psl output/exludeself_nonover_mrna1.psl
	${DIFF} expected/exludeself_nonover_mrna1.psl output/exludeself_nonover_mrna1.psl

# Test of -excludeSelf with file containing 3 overlap mRNAs and 1
# non-overlaping.  Overlaping should be selected, since they are overlapped by
# more than self.
exludeself_over_mrna2: mkout
	${OVERSEL} -excludeSelf input/mrna-self2.psl input/mrna-self2.psl output/exludeself_over_mrna2.psl
	${DIFF} expected/exludeself_over_mrna2.psl output/exludeself_over_mrna2.psl

# Test of -excludeSelf and -nonOverlaping with file containing 3 overlap mRNAs
# and 1 non-overlaping.  Only non-overlaping should be selected.
exludeself_nonover_mrna2: mkout
	${OVERSEL} -excludeSelf -nonOverlaping input/mrna-self2.psl input/mrna-self2.psl output/exludeself_nonover_mrna2.psl
	${DIFF} expected/exludeself_nonover_mrna2.psl output/exludeself_nonover_mrna2.psl

# select by column
colSelect: mkout
	${OVERSEL} -selectCoordCols=1,2,3 -inCoordCols=3,4,5 \
	    input/gene.select input/gene.clusters output/colSelect.cluster
	${DIFF} expected/colSelect.cluster output/colSelect.cluster

# exclude by column
colExclude: mkout
	${OVERSEL} -nonOverlaping -selectCoordCols=1,2,3 -inCoordCols=3,4,5 \
	    input/gene.select input/gene.clusters output/colExclude.cluster
	${DIFF} expected/colExclude.cluster output/colExclude.cluster

# CDS doesn't overlap, reset is the same
cdsOverlapCdsCds: mkout
	${OVERSEL} -inCds -selectCds input/cdsOverlap1.gp input/cdsOverlap2.gp output/cdsOverlapCdsCds.gp
	${DIFF} expected/cdsOverlapCdsCds.gp output/cdsOverlapCdsCds.gp

cdsOverlapCdsExon: mkout
	${OVERSEL} -inCds input/cdsOverlap1.gp input/cdsOverlap2.gp output/cdsOverlapCdsExon.gp
	${DIFF} expected/cdsOverlapCdsExon.gp output/cdsOverlapCdsExon.gp

cdsOverlapExonCds: mkout
	${OVERSEL} -selectCds input/cdsOverlap1.gp input/cdsOverlap2.gp output/cdsOverlapExonCds.gp
	${DIFF} expected/cdsOverlapExonCds.gp output/cdsOverlapExonCds.gp

mkout:
	@${MKDIR} output

clean:
	rm -rf output
