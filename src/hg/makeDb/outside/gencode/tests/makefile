kentSrc = ../../../../..
include ${kentSrc}/inc/common.mk
host=$(shell hostname)
ppid=$(shell echo $$PPID)
tmpExt = ${host}.${ppid}.tmp

## Notes:
# set1 includes PAR gene with renaming on chrY ENSG00000182484 -> ENSGR0000182484
##

diff = diff -u

gencodeGxfToAttrs = ../bin/gencodeGxfToAttrs
ensToUcscMkLift = ../bin/ensToUcscMkLift
gencodeGxfToGenePred = ../bin/gencodeGxfToGenePred
gencodeExonSupportToTable = ../bin/gencodeExonSupportToTable

ucscGenomeDbHs37 = hg19
ucscGenomeDbHs38 = hg38
ucscGenomeDbMm38 = mm10

hg37AsmReport = input/GCF_000001405.25_GRCh37.p13_assembly_report.txt
hg38AsmReport = input/GCF_000001405.39_GRCh38.p13_assembly_report.txt 
mm38AsmReport = input/GCF_000001635.26_GRCm38.p6_assembly_report.txt

ensToUcscMappingChainHg37 = output/ensToUcscMappingHg37.chain
ensToUcscMappingChainHg38 = output/ensToUcscMappingHg38.chain

all::

test::  attrsTests ensToUcscMkLiftTests genePredTests exonSupportTests

##
# attributes tests
##
attrsTests: attrsSet1GtfTest attrsV21GtfTest attrsV21Gff3Test attrsV27ParGtfTest attrsV27ParGff3Test \
	attrsV19GtfTest attrsTagGtfTests attrsTagGff3Tests attrsPseudoGff3Tests

attrsSet1GtfTest: mkdirs
	${gencodeGxfToAttrs} input/set1.gtf output/$@.tsv output/$@.attrs output/$@.tsl output/$@.tags
	${diff} expected/$@.tsv output/$@.tsv
	${diff} expected/$@.attrs output/$@.attrs
	${diff} expected/$@.tsl output/$@.tsl
	${diff} expected/$@.tags output/$@.tags

attrsV21GtfTest: mkdirs
	${gencodeGxfToAttrs} input/v21.gtf output/$@.tsv output/$@.attrs output/$@.tsl output/$@.tags
	${diff} expected/$@.tsv output/$@.tsv
	${diff} expected/$@.attrs output/$@.attrs
	${diff} expected/$@.tsl output/$@.tsl
	${diff} expected/$@.tags output/$@.tags

attrsV21Gff3Test: mkdirs
	${gencodeGxfToAttrs} input/v21.gff3 output/$@.tsv output/$@.attrs output/$@.tsl output/$@.tags
	${diff} expected/$@.tsv output/$@.tsv
	${diff} expected/$@.attrs output/$@.attrs
	${diff} expected/$@.tsl output/$@.tsl
	${diff} expected/$@.tags output/$@.tags

attrsV27ParGtfTest: mkdirs
	${gencodeGxfToAttrs} input/v27.par.gtf output/$@.tsv output/$@.attrs output/$@.tsl output/$@.tags
	${diff} expected/$@.tsv output/$@.tsv
	${diff} expected/$@.attrs output/$@.attrs
	${diff} expected/$@.tsl output/$@.tsl
	${diff} expected/$@.tags output/$@.tags

attrsV27ParGff3Test:mkdirs
	${gencodeGxfToAttrs} input/v27.par.gff3 output/$@.tsv output/$@.attrs output/$@.tsl output/$@.tags
	${diff} expected/$@.tsv output/$@.tsv
	${diff} expected/$@.attrs output/$@.attrs
	${diff} expected/$@.tsl output/$@.tsl
	${diff} expected/$@.tags output/$@.tags

# compatiblity with V19 for reloading
attrsV19GtfTest: mkdirs
	${gencodeGxfToAttrs} input/v19.gtf output/$@.tsv output/$@.attrs output/$@.tsl output/$@.tags
	${diff} expected/$@.tsv output/$@.tsv
	${diff} expected/$@.attrs output/$@.attrs
	${diff} expected/$@.tsl output/$@.tsl
	${diff} expected/$@.tags output/$@.tags

attrsTagGtfTests: mkdirs
	${gencodeGxfToAttrs} input/tags.gtf output/$@.tsv output/$@.attrs output/$@.tsl output/$@.tags
	${diff} expected/$@.tsv output/$@.tsv
	${diff} expected/$@.attrs output/$@.attrs
	${diff} expected/$@.tsl output/$@.tsl
	${diff} expected/$@.tags output/$@.tags

attrsTagGff3Tests: mkdirs
	${gencodeGxfToAttrs} input/tags.gff3 output/$@.tsv output/$@.attrs output/$@.tsl output/$@.tags
	${diff} expected/$@.tsv output/$@.tsv
	${diff} expected/$@.attrs output/$@.attrs
	${diff} expected/$@.tsl output/$@.tsl
	${diff} expected/$@.tags output/$@.tags


# ENST00000422009.5 pseudo was ending up with wrong transcriptClass
attrsPseudoGff3Tests: mkdirs
	${gencodeGxfToAttrs} input/pseudo.gff3 output/$@.tsv output/$@.attrs output/$@.tsl output/$@.tags
	${diff} expected/$@.tsv output/$@.tsv
	${diff} expected/$@.attrs output/$@.attrs
	${diff} expected/$@.tsl output/$@.tsl
	${diff} expected/$@.tags output/$@.tags

###
# ensembl/gencode to UCSC liftover
#
# Note: these tests compare hgwdev chromInfo to the assembly reports files in input.  If
# chromInfo has been updated because of a new path, the assembly reports needs to be updated.
###
ensToUcscMkLiftTests: \
	ensToUcscMkLiftHs37Test \
	ensToUcscMkLiftHs38Test \
	ensToUcscMkLiftMm38Test

# hg19/GRCh37
ensToUcscMkLiftHs37Test: mkdirs
	${ensToUcscMkLift} ${ucscGenomeDbHs37} ${hg37AsmReport} output/$@.chain
	${diff} expected/$@.chain output/$@.chain

# hg38/GRCh38
ensToUcscMkLiftHs38Test: mkdirs
	${ensToUcscMkLift} ${ucscGenomeDbHs38} ${hg38AsmReport} output/$@.chain
	chainStats output/$@.chain >/dev/null
	${diff} expected/$@.chain output/$@.chain

# mm10/GRCm38
ensToUcscMkLiftMm38Test: mkdirs
	${ensToUcscMkLift} ${ucscGenomeDbMm38} ${mm38AsmReport} output/$@.chain
	chainStats output/$@.chain >/dev/null
	${diff} expected/$@.chain output/$@.chain

###
# Creating genePred track
###
genePredTests: set1AnnotTest ummappingWarnHs38Test specialCases1Test

set1AnnotTest: ${set1Gp} ${ensToUcscMappingChainHg37} mkdirs
	${gencodeGxfToGenePred} ${ucscGenomeDbHs37} input/set1.gtf ${ensToUcscMappingChainHg37} output/$@.gp
	${diff} expected/$@.gp output/$@.gp

ummappingWarnHs38Test: ${ensToUcscMappingChainHg38}
	${gencodeGxfToGenePred} ${ucscGenomeDbHs38} input/mappingErrorsHs38.gtf ${ensToUcscMappingChainHg38} output/$@.gp --unmapped=output/$@.unmapped.gp >output/$@.out 2>&1
	${diff} expected/$@.out output/$@.out
	${diff} expected/$@.gp output/$@.gp
	${diff} expected/$@.unmapped.gp output/$@.unmapped.gp

# chrM, PAR, mapped and unmapped haplotypes and patches
specialCases1Test: ${ensToUcscMappingChainHg37} mkdirs
	${gencodeGxfToGenePred} --unmapped=output/$@.unmapped.gp ${ucscGenomeDbHs37} input/specialCases1.gtf ${ensToUcscMappingChainHg37} output/$@.gp
	${diff} expected/$@.gp output/$@.gp
	${diff} expected/$@.unmapped.gp output/$@.unmapped.gp

##
# exon support
##
exonSupportTests: exonSupport1Test

exonSupport1Test: ${ensToUcscMappingChainHg37}
	${gencodeExonSupportToTable} input/examples1.Exon_supporting_feature ${ensToUcscMappingChainHg37} output/$@.exons
	${diff} expected/$@.exons output/$@.exons

##
# mapping chains for tests
##
testChains: ${ensToUcscMappingChainHg37} ${ensToUcscMappingChainHg38}
${ensToUcscMappingChainHg37}: mkdirs
	${ensToUcscMkLift} ${ucscGenomeDbHs37} ${hg37AsmReport} $@.${tmpExt}
	mv -f $@.${tmpExt} $@

${ensToUcscMappingChainHg38}: mkdirs
	${ensToUcscMkLift} ${ucscGenomeDbHs38} ${hg38AsmReport} $@.${tmpExt}
	mv -f $@.${tmpExt} $@

mkdirs:
	@mkdir -p output

clean::
	rm -rf output
