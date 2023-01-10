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
buildGencodeToUcscLift = ../bin/buildGencodeToUcscLift
gencodeGxfToGenePred = ../bin/gencodeGxfToGenePred
gencodeExonSupportToTable = ../bin/gencodeExonSupportToTable
gencodeMakeTracks = ../bin/gencodeMakeTracks
gencodeMakeAttrs = ../bin/gencodeMakeAttrs
gencodePolyaGxfToGenePred = ../bin/gencodePolyaGxfToGenePred

gencodeToUcscMappingChainHg37 = output/gencodeToUcscMappingHg37.chain
gencodeToUcscMappingChainHg38 = output/gencodeToUcscMappingHg38.chain

all::

test:: attrsTests makeTracksTests makeAttrsTests buildGencodeToUcscLiftTests \
	genePredTests exonSupportTests polyATests

##
# attributes tests
##
attrsTests: attrsSet1GtfTest attrsV21GtfTest attrsV21Gff3Test attrsV27ParGtfTest attrsV27ParGff3Test \
	attrsV19GtfTest attrsTagGtfTests attrsTagGff3Tests attrsPseudoGff3Tests \
	attrsErrorGtfTest attrsErrorGff3Test attrsRankTest

attrsSet1GtfTest: mkdirs
	${gencodeGxfToAttrs} input/set1.gtf output/$@.tsv
	${diff} expected/$@.tsv output/$@.tsv

attrsV21GtfTest: mkdirs
	${gencodeGxfToAttrs} input/v21.gtf output/$@.tsv
	${diff} expected/$@.tsv output/$@.tsv

attrsV21Gff3Test: mkdirs
	${gencodeGxfToAttrs} input/v21.gff3 output/$@.tsv
	${diff} expected/$@.tsv output/$@.tsv

attrsV27ParGtfTest: mkdirs
	${gencodeGxfToAttrs} input/v27.par.gtf output/$@.tsv
	${diff} expected/$@.tsv output/$@.tsv

attrsV27ParGff3Test:mkdirs
	${gencodeGxfToAttrs} input/v27.par.gff3 output/$@.tsv
	${diff} expected/$@.tsv output/$@.tsv

# compatiblity with V19 for reloading
attrsV19GtfTest: mkdirs
	${gencodeGxfToAttrs} input/v19.gtf output/$@.tsv
	${diff} expected/$@.tsv output/$@.tsv

attrsTagGtfTests: mkdirs
	${gencodeGxfToAttrs} input/tags.gtf output/$@.tsv
	${diff} expected/$@.tsv output/$@.tsv

attrsTagGff3Tests: mkdirs
	${gencodeGxfToAttrs} input/tags.gff3 output/$@.tsv
	${diff} expected/$@.tsv output/$@.tsv


# ENST00000422009.5 pseudo was ending up with wrong transcriptClass
attrsPseudoGff3Tests: mkdirs
	${gencodeGxfToAttrs} input/pseudo.gff3 output/$@.tsv
	${diff} expected/$@.tsv output/$@.tsv

attrsErrorGtfTest: mkdirs
	if ${gencodeGxfToAttrs} input/attrErrors.gtf /dev/null >output/$@.err 2>&1 ; \
	   then echo Error: should fail >&2 ; exit 1 ; else exit 0 ; fi
	${diff} expected/$@.err output/$@.err

attrsErrorGff3Test: mkdirs
	if ${gencodeGxfToAttrs} input/attrErrors.gff3 /dev/null >output/$@.err 2>&1 ; \
	   then echo Error: should fail >&2 ; exit 1 ; else exit 0 ; fi
	${diff} expected/$@.err output/$@.err

attrsRankTest: mkdirs
	${gencodeGxfToAttrs} --transcriptRanks=input/v42.transcript_rankings.txt input/v42.gff3 output/$@.tsv
	${diff} expected/$@.tsv output/$@.tsv



##
# generate input need for tests
#
set1Gp = output/set1.gp
set1Attrs = output/set1.attrs.tsv

${set1Gp}: input/set1.gtf ${gencodeToUcscMappingChainHg37} mkdirs
	${gencodeGxfToGenePred} hg19 input/set1.gtf ${gencodeToUcscMappingChainHg37} $@.tmp
	mv -f $@.tmp $@

${set1Attrs}: input/set1.gtf mkdirs
	${gencodeGxfToAttrs} $< $@.tmp
	mv -f $@.tmp $@

##
# generate tracks from data parse from gencode
#
makeTracksTests: makeTracksPseudoTest makeTracksBasicTest makeTracksCompTest

makeTracksPseudoTest: ${set1Gp} ${set1Attrs} mkdirs
	${gencodeMakeTracks} pseudogene ${set1Gp} ${set1Attrs} output/$@.gp
	${diff} expected/$@.gp output/$@.gp

makeTracksBasicTest: ${set1Gp} ${set1Attrs} mkdirs
	${gencodeMakeTracks} basic ${set1Gp} ${set1Attrs} output/$@.gp
	${diff} expected/$@.gp output/$@.gp

makeTracksCompTest: ${set1Gp} ${set1Attrs} mkdirs
	${gencodeMakeTracks} comp ${set1Gp} ${set1Attrs} output/$@.gp
	${diff} expected/$@.gp output/$@.gp

##
# generate attribute tracks from parsed data.
#
makeAttrsTests: makeAttrsTest

makeAttrsTest: ${set1Gp} ${set1Attrs} mkdirs
	${gencodeMakeAttrs} ${set1Gp} ${set1Attrs} output/$@.attrs.tab output/$@.tags.tab output/$@.tsl.tab
	${diff} expected/$@.attrs.tab output/$@.attrs.tab
	${diff} expected/$@.tags.tab output/$@.tags.tab
	${diff} expected/$@.tsl.tab output/$@.tsl.tab

###
# GENCODE to UCSC liftover
#
# Note: these tests compare hgwdev chromInfo to the assembly reports files in input.  If
# chromInfo has been updated because of a new path, the assembly reports needs to be updated.
###
buildGencodeToUcscLiftTests: \
	buildGencodeToUcscLiftHs37Test \
	buildGencodeToUcscLiftHs38Test \
	buildGencodeToUcscLiftMm38Test

# hg19/GRCh37
buildGencodeToUcscLiftHs37Test: mkdirs
	${buildGencodeToUcscLift} hg19 output/$@.chain
	${diff} expected/$@.chain output/$@.chain

# hg38/GRCh38
buildGencodeToUcscLiftHs38Test: mkdirs
	${buildGencodeToUcscLift} hg38 output/$@.chain
	chainStats output/$@.chain >/dev/null
	${diff} expected/$@.chain output/$@.chain

# mm10/GRCm38
buildGencodeToUcscLiftMm38Test: mkdirs
	${buildGencodeToUcscLift} mm10 output/$@.chain
	chainStats output/$@.chain >/dev/null
	${diff} expected/$@.chain output/$@.chain

###
# Creating genePred file
###
genePredTests: set1AnnotTest ummappingWarnHs38Test badMappedErrorsHs38Test specialCases1Test

set1AnnotTest: ${set1Gp} ${gencodeToUcscMappingChainHg37} mkdirs
	${gencodeGxfToGenePred} hg19 input/set1.gtf ${gencodeToUcscMappingChainHg37} output/$@.gp
	${diff} expected/$@.gp output/$@.gp

ummappingWarnHs38Test: ${gencodeToUcscMappingChainHg38}
	${gencodeGxfToGenePred} --warnAndContinue hg38 input/missingSeqsHs38.gtf ${gencodeToUcscMappingChainHg38} output/$@.gp --unmapped=output/$@.unmapped.gp >output/$@.out 2>&1
	${diff} expected/$@.out output/$@.out
	${diff} expected/$@.gp output/$@.gp
	${diff} expected/$@.unmapped.gp output/$@.unmapped.gp

badMappedErrorsHs38Test: ${gencodeToUcscMappingChainHg38}
	if ${gencodeGxfToGenePred} --warnAndContinue hg38 input/mappingErrorsHs38.gtf ${gencodeToUcscMappingChainHg38} output/$@.gp --unmapped=output/$@.unmapped.gp >output/$@.out 2>&1 ; \
	   then echo Error: should fail >&2 ; exit 1 ; else exit 0 ; fi
	${diff} expected/$@.out output/$@.out

# chrM, PAR, mapped and unmapped haplotypes and patches
specialCases1Test: ${gencodeToUcscMappingChainHg37} mkdirs
	${gencodeGxfToGenePred} --unmapped=output/$@.unmapped.gp hg19 input/specialCases1.gtf ${gencodeToUcscMappingChainHg37} output/$@.gp
	${diff} expected/$@.gp output/$@.gp
	${diff} expected/$@.unmapped.gp output/$@.unmapped.gp

##
# exon support
##
exonSupportTests: exonSupport1Test

exonSupport1Test: ${gencodeToUcscMappingChainHg37}
	${gencodeExonSupportToTable} input/examples1.Exon_supporting_feature ${gencodeToUcscMappingChainHg37} output/$@.exons
	${diff} expected/$@.exons output/$@.exons

polyATests: ployAGpTest

ployAGpTest: mkdirs ${gencodeToUcscMappingChainHg38}
	${gencodePolyaGxfToGenePred} input/polyA.gff3 ${gencodeToUcscMappingChainHg38} output/$@.gp
	${diff} expected/$@.gp output/$@.gp

##
# mapping chains for tests
##
testChains: ${gencodeToUcscMappingChainHg37} ${gencodeToUcscMappingChainHg38}
${gencodeToUcscMappingChainHg37}: mkdirs
	${buildGencodeToUcscLift} hg19 $@.${tmpExt}
	mv -f $@.${tmpExt} $@

${gencodeToUcscMappingChainHg38}: mkdirs
	${buildGencodeToUcscLift} hg38 $@.${tmpExt}
	mv -f $@.${tmpExt} $@

mkdirs:
	@mkdir -p output

clean::
	rm -rf output
