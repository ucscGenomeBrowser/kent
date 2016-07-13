kentSrc = ../../..
include ../../../inc/common.mk

hgGetAnn = ${DESTBINDIR}/hgGetAnn

# use hg15 to avoid incremental update
DB=hg15

all:

test: chrRangeTests textTests

###
# chrom range queries
###
CHR_RANGE = chr5:112,253,464-112,324,832

chrRangeTests: chrRangeGenePred chrRangePsl chrRangePslSplit chrRangePslKeepBin \
	chrRangeGenePredColHeaders chrRangeGenePredTsvHeaders

# range genePred
chrRangeGenePred: mkdirs
	${hgGetAnn} ${DB} refGene ${CHR_RANGE} stdout | sort > output/$@.gp
	sort expected/$@.gp | diff - output/$@.gp

# range psl single
chrRangePsl: mkdirs
	${hgGetAnn} ${DB} refSeqAli ${CHR_RANGE} output/$@.psl
	diff expected/$@.psl output/$@.psl

# range psl split
chrRangePslSplit: mkdirs
	${hgGetAnn} ${DB} intronEst chr4:56,225,976-56,230,912 output/$@.psl
	diff expected/$@.psl output/$@.psl

# range psl keep bin, include spaces in query
chrRangePslKeepBin: mkdirs
	${hgGetAnn} -keepBin ${DB} blatFugu "chr5 112272300 112280228" output/$@.psl
	diff expected/$@.psl output/$@.psl

# range genePred with headers
chrRangeGenePredColHeaders: mkdirs
	${hgGetAnn} -colHeaders ${DB} refGene ${CHR_RANGE} stdout | sort > output/$@.gp
	sort expected/$@.gp | diff - output/$@.gp

# range genePred with tsv headers
chrRangeGenePredTsvHeaders: mkdirs
	${hgGetAnn} -tsvHeaders ${DB} refGene ${CHR_RANGE} stdout | sort > output/$@.gp
	sort expected/$@.gp | diff - output/$@.gp

###
# acc, name and textual queries
###
textTests: geneNameQuery geneAccQuery mrnaAccQuery keywordQuery multSameLoc

geneNameQuery:
	${hgGetAnn} ${DB} refGene CLOCK output/$@.gp
	diff expected/$@.gp output/$@.gp

# aligns in 9 places
geneAccQuery:
	${hgGetAnn} ${DB} refGene NM_005083 output/$@.gp
	diff expected/$@.gp output/$@.gp

# aligns in 10 places
mrnaAccQuery:
	${hgGetAnn} ${DB} mrna AB062075  output/$@.psl
	diff expected/$@.psl output/$@.psl

# hits multiple mRNAs
keywordQuery::
	${hgGetAnn} ${DB} mrna "breast cancer early onset" output/$@.psl
	diff expected/$@.psl output/$@.psl

# acc query were there are multiple genes with same start/stop
multSameLoc:
	${hgGetAnn} ${DB} refGene NM_005522 output/$@.gp
	diff expected/$@.gp output/$@.gp


clean::
	rm -rf output

mkdirs:
	@${MKDIR} output
