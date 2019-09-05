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

ucscGenomeDbHs37 = hg19
ucscGenomeDbHs38 = hg38
ucscGenomeDbMm38 = mm10

ensToUcscMappingChainHg37 = output/ensToUcscMappingHg37.chain
ensToUcscMappingChainHg38 = output/ensToUcscMappingHg38.chain

test::  attrsTests ensToUcscMkLiftTests genePredTest

##
# attributes tests
##
attrsTests: attrsSet1GtfTest attrsV21GtfTest attrsV21Gff3Test attrsV27ParGtfTest attrsV27ParGff3Test attrsV19GtfTest

attrsSet1GtfTest: mkdirs
	${gencodeGxfToAttrs} input/set1.gtf output/$@.attrs output/$@.tsl
	${diff} expected/$@.attrs output/$@.attrs
	${diff} expected/$@.tsl output/$@.tsl

attrsV21GtfTest: mkdirs
	${gencodeGxfToAttrs} input/v21.gtf output/$@.attrs output/$@.tsl
	${diff} expected/$@.attrs output/$@.attrs
	${diff} expected/$@.tsl output/$@.tsl

attrsV21Gff3Test: mkdirs
	${gencodeGxfToAttrs} input/v21.gff3 output/$@.attrs output/$@.tsl
	${diff} expected/$@.attrs output/$@.attrs
	${diff} expected/$@.tsl output/$@.tsl

attrsV27ParGtfTest: mkdirs
	${gencodeGxfToAttrs} input/v27.par.gtf output/$@.attrs output/$@.tsl
	${diff} expected/$@.attrs output/$@.attrs
	${diff} expected/$@.tsl output/$@.tsl

attrsV27ParGff3Test:mkdirs
	${gencodeGxfToAttrs} input/v27.par.gff3 output/$@.attrs output/$@.tsl
	${diff} expected/$@.attrs output/$@.attrs
	${diff} expected/$@.tsl output/$@.tsl

# compatiblity with V19 for reloading
attrsV19GtfTest: mkdirs
	${gencodeGxfToAttrs} input/v19.gtf output/$@.attrs output/$@.tsl
	${diff} expected/$@.attrs output/$@.attrs
	${diff} expected/$@.tsl output/$@.tsl

###
# ensembl/gencode to UCSC liftover
###
ensToUcscMkLiftTests: \
	ensToUcscMkLiftHs37Test \
	ensToUcscMkLiftHs38Test \
	ensToUcscMkLiftMm38Test



# hg19/GRCh37
ensToUcscMkLiftHs37Test: mkdirs
	${ensToUcscMkLift} ${ucscGenomeDbHs37} output/$@.chain
	${diff} expected/$@.chain output/$@.chain

# hg38/GRCh38
ensToUcscMkLiftHs38Test: mkdirs
	${ensToUcscMkLift} ${ensemblCDnaDbHs38} ${grcRefAssemblyHs38} ${ucscGenomeDbHs38} output/$@.chain
	chainStats output/$@.chain >/dev/null
	${diff} expected/$@.chain output/$@.chain

# mm10/GRCm38
ensToUcscMkLiftMm38Test: mkdirs
	${ensToUcscMkLift} ${ensemblCDnaDbMm38} ${grcRefAssemblyMm38} ${ucscGenomeDbMm38} output/$@.chain
	chainStats output/$@.chain >/dev/null
	${diff} expected/$@.chain output/$@.chain

###
# Creating genePred track
###
genePredTests: set1AnnotTest ummappingWarnHs38Test specialCases1Test

set1AnnotTest: ${set1Gp} mkdirs
	${gencodeGxfToGenePred} input/set1.gtf ${ensToUcscMappingChainHg38} output/$@.gp
	${diff} expected/$@.gp output/$@.gp

ummappingWarnHs38Test: ${ensToUcscMappingChainHg38}
	${gencodeGxfToGenePred} input/mappingErrorsHs38.gtf ${ensToUcscMappingChainHg38} output/$@.gp --unmapped=output/$@.unmapped.gp >output/$@.out 2>&1
	${diff} expected/$@.out output/$@.out
	${diff} expected/$@.gp output/$@.gp
	${diff} expected/$@.unmapped.gp output/$@.unmapped.gp

# chrM, PAR, mapped and unmapped haplotypes and patches
specialCases1Test: ${ensToUcscMappingChainHg37} mkdirs
	${gencodeGxfToGenePred} --unmapped=output/$@.unmapped.gp input/specialCases1.gtf ${ensToUcscMappingChainHg37} output/$@.gp
	${diff} expected/$@.gp output/$@.gp
	${diff} expected/$@.unmapped.gp output/$@.unmapped.gp



##
# mapping chains for tests
##
testChains: ${ensToUcscMappingChainHg37} ${ensToUcscMappingChainHg38}
${ensToUcscMappingChainHg37}: mkdirs
	${ensToUcscMkLift} ${ucscGenomeDbHs37} $@.${tmpExt}
	mv -f $@.${tmpExt} $@

${ensToUcscMappingChainHg38}: mkdirs
	${ensToUcscMkLift} ${ucscGenomeDbHs38}  $@.${tmpExt}
	mv -f $@.${tmpExt} $@

mkdirs:
	@mkdir -p output

clean::
	rm -rf output
