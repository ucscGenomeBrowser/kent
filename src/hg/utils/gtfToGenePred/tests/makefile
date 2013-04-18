include ../../../../inc/common.mk

all:

test: basic srcPre dups impliedStop ensembl geneNameAsName2 splitStop ignoreGroupWithoutExons

# basic conversion
basic: mkdirs
	gtfToGenePred -infoOut=output/$@.info input/basic.gtf output/$@.gp
	genePredCheck -verbose=0 output/$@.gp
	diff expected/$@.gp output/$@.gp
	diff expected/$@.info output/$@.info

# source prefix filtering
srcPre: mkdirs
	gtfToGenePred -infoOut=output/$@.info -sourcePrefix=mgc2 -sourcePrefix=mgc3 input/basic.gtf output/$@.gp
	genePredCheck -verbose=0 output/$@.gp
	diff expected/$@.gp output/$@.gp
	diff expected/$@.info output/$@.info

# error reporting
dups: mkdirs
	if gtfToGenePred -allErrors input/dups.gtf output/$@.gp >output/$@.out 2>&1 ; then false ; else true ; fi
	diff expected/$@.out output/$@.out
	diff expected/$@.gp output/$@.gp

impliedStop: mkdirs
	gtfToGenePred -impliedStopAfterCds -infoOut=output/$@.info input/flybase.gtf output/$@.gp
	genePredCheck -verbose=0 output/$@.gp
	diff expected/$@.gp output/$@.gp
	diff expected/$@.info output/$@.info

# protein id
ensembl: mkdirs
	gtfToGenePred -genePredExt -infoOut=output/$@.info input/ensembl.gtf output/$@.gp
	genePredCheck -verbose=0 output/$@.gp
	diff expected/$@.gp output/$@.gp
	diff expected/$@.info output/$@.info

# -geneNameAsName2
geneNameAsName2: mkdirs
	gtfToGenePred -genePredExt -geneNameAsName2 input/ensembl.gtf output/$@.gp
	genePredCheck -verbose=0 output/$@.gp
	diff expected/$@.gp output/$@.gp

# split stop coding, produced bogus frame
splitStop: mkdirs
	gtfToGenePred -genePredExt input/splitStop.gtf output/$@.gp
	genePredCheck -verbose=0 output/$@.gp
	diff expected/$@.gp output/$@.gp

ignoreGroupWithoutExons: mkdirs
	gtfToGenePred -genePredExt -ignoreGroupsWithoutExons input/geneFeature.gtf output/$@.gp
	genePredCheck -verbose=0 output/$@.gp
	diff expected/$@.gp output/$@.gp


mkdirs:
	mkdir -p output

clean:
	rm -rf output
