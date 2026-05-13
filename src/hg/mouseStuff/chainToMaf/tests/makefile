kentSrc = ../../../..
include ${kentSrc}/inc/common.mk

chainToMaf = ${DESTBINDIR}/chainToMaf
chainToAxt = ${DESTBINDIR}/chainToAxt
axtToMaf = ${DESTBINDIR}/axtToMaf
twoBitInfo = ${DESTBINDIR}/twoBitInfo

hg38TwoBit = /gbdb/hg38/hg38.2bit
mm10TwoBit = /gbdb/mm10/mm10.2bit
hg38TwoBitUrl = https://hgdownload.gi.ucsc.edu/gbdb/hg38/hg38.2bit
mm10TwoBitUrl = https://hgdownload.gi.ucsc.edu/gbdb/mm10/mm10.2bit

all:

test: localNoPrefix localPrefix urlPrefix rescorePrefix viaAxt
	${MAKE} clean

# basic local 2bit test, no prefixes
localNoPrefix: mkdirs
	${chainToMaf} input/hg38.mm10.test.chain ${hg38TwoBit} ${mm10TwoBit} output/$@.maf
	diff -u expected/$@.maf output/$@.maf

# local 2bit with target/query prefixes
localPrefix: mkdirs
	${chainToMaf} -tPrefix=hg38. -qPrefix=mm10. \
	    input/hg38.mm10.test.chain ${hg38TwoBit} ${mm10TwoBit} output/$@.maf
	diff -u expected/$@.maf output/$@.maf

# remote 2bit URLs with prefixes
urlPrefix: mkdirs
	${chainToMaf} -tPrefix=hg38. -qPrefix=mm10. \
	    input/hg38.mm10.test.chain ${hg38TwoBitUrl} ${mm10TwoBitUrl} output/$@.maf
	diff -u expected/$@.maf output/$@.maf

# rescore with prefixes
rescorePrefix: mkdirs
	${chainToMaf} -score -tPrefix=hg38. -qPrefix=mm10. \
	    input/hg38.mm10.test.chain ${hg38TwoBit} ${mm10TwoBit} output/$@.maf
	diff -u expected/$@.maf output/$@.maf

# verify chainToMaf output matches the chainToAxt | axtToMaf pipeline.
# uses the same input chain and 2bits as localNoPrefix.
viaAxt: mkdirs
	${twoBitInfo} ${hg38TwoBit} output/hg38.sizes
	${twoBitInfo} ${mm10TwoBit} output/mm10.sizes
	${chainToAxt} input/hg38.mm10.test.chain ${hg38TwoBit} ${mm10TwoBit} \
	    output/$@.axt
	${axtToMaf} output/$@.axt output/hg38.sizes output/mm10.sizes \
	    output/$@.viaPipe.maf
	${chainToMaf} input/hg38.mm10.test.chain ${hg38TwoBit} ${mm10TwoBit} \
	    output/$@.direct.maf
	diff -u output/$@.viaPipe.maf output/$@.direct.maf

clean::
	rm -rf output

mkdirs:
	@${MKDIR} output
