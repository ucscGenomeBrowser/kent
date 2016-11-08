kentSrc = ../../../..
include ../../../../inc/common.mk

gtfToGenePred = ${DESTBINDIR}/gtfToGenePred
genePredCheck = ${DESTBINDIR}/genePredCheck

all:

test: basic srcPre dups impliedStop ensembl geneNameAsName2 splitStop \
	ignoreGroupWithoutExons ensemblSplicedStop ensemblWithVersions ensemblWithVersionsGeneName2

# basic conversion
basic: mkdirs
	${gtfToGenePred} -infoOut=output/$@.info input/basic.gtf output/$@.gp
	${genePredCheck} -verbose=0 output/$@.gp
	diff expected/$@.gp output/$@.gp
	diff expected/$@.info output/$@.info

# source prefix filtering
srcPre: mkdirs
	${gtfToGenePred} -infoOut=output/$@.info -sourcePrefix=mgc2 -sourcePrefix=mgc3 input/basic.gtf output/$@.gp
	${genePredCheck} -verbose=0 output/$@.gp
	diff expected/$@.gp output/$@.gp
	diff expected/$@.info output/$@.info

# error reporting
dups: mkdirs
	if ${gtfToGenePred} -allErrors input/dups.gtf output/$@.gp >output/$@.out 2>&1 ; then false ; else true ; fi
	diff expected/$@.out output/$@.out
	diff expected/$@.gp output/$@.gp

impliedStop: mkdirs
	${gtfToGenePred} -impliedStopAfterCds -infoOut=output/$@.info input/flybase.gtf output/$@.gp
	${genePredCheck} -verbose=0 output/$@.gp
	diff expected/$@.gp output/$@.gp
	diff expected/$@.info output/$@.info

# protein id, make sure -inclVersion does nothing for this file
ensembl: mkdirs
	${gtfToGenePred} -genePredExt -infoOut=output/$@.info -includeVersion input/ensembl.gtf output/$@.gp
	${genePredCheck} -verbose=0 output/$@.gp
	diff expected/$@.gp output/$@.gp
	diff expected/$@.info output/$@.info

# -geneNameAsName2, make sure -inclVersion does nothing for this file
geneNameAsName2: mkdirs
	${gtfToGenePred} -genePredExt -geneNameAsName2 -includeVersion input/ensembl.gtf output/$@.gp
	${genePredCheck} -verbose=0 output/$@.gp
	diff expected/$@.gp output/$@.gp

# split stop coding, produced bogus frame
splitStop: mkdirs
	${gtfToGenePred} -genePredExt input/splitStop.gtf output/$@.gp
	${genePredCheck} -verbose=0 output/$@.gp
	diff expected/$@.gp output/$@.gp

ignoreGroupWithoutExons: mkdirs
	${gtfToGenePred} -genePredExt -ignoreGroupsWithoutExons -infoOut=output/$@.info input/geneFeature.gtf output/$@.gp
	${genePredCheck} -verbose=0 output/$@.gp
	diff expected/$@.gp output/$@.gp
	diff expected/$@.info output/$@.info

# this caused assert in assigning exon frame
ensemblSplicedStop: mkdirs
	${gtfToGenePred} -genePredExt input/ensemblSplicedStop.gtf output/$@.gp
	${genePredCheck} -verbose=0 output/$@.gp
	diff expected/$@.gp output/$@.gp

# including version numbers
ensemblWithVersions: mkdirs
	${gtfToGenePred} -genePredExt -includeVersion -infoOut=output/$@.info input/ensemblWithVersions.gtf output/$@.gp
	${genePredCheck} -verbose=0 output/$@.gp
	diff expected/$@.gp output/$@.gp
	diff expected/$@.info output/$@.info

# including version numbers with -geneNameAsName2
ensemblWithVersionsGeneName2: mkdirs
	${gtfToGenePred} -genePredExt -geneNameAsName2 -includeVersion -infoOut=output/$@.info input/ensemblWithVersions.gtf output/$@.gp
	${genePredCheck} -verbose=0 output/$@.gp
	diff expected/$@.gp output/$@.gp
	diff expected/$@.info output/$@.info

mkdirs:
	mkdir -p output

clean:
	rm -rf output
