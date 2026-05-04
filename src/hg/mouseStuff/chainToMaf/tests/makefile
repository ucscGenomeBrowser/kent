kentSrc = ../../../..
include ${kentSrc}/inc/common.mk

chainToMaf = ${DESTBINDIR}/chainToMaf

hg38TwoBit = /gbdb/hg38/hg38.2bit
mm10TwoBit = /gbdb/mm10/mm10.2bit
hg38TwoBitUrl = https://hgdownload.gi.ucsc.edu/gbdb/hg38/hg38.2bit
mm10TwoBitUrl = https://hgdownload.gi.ucsc.edu/gbdb/mm10/mm10.2bit

all:

test: localNoPrefix localPrefix urlPrefix rescorePrefix
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

clean::
	rm -rf output

mkdirs:
	@${MKDIR} output
