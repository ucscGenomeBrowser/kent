kentSrc = ../../../..
include ${kentSrc}/inc/common.mk

agpAlign = ${DESTBINDIR}/agpAlign

all:

test: testHg19ToHg38 testHg38ToHg19
	${MAKE} clean

testHg19ToHg38: mkdirs
	${agpAlign} input/test.{hg19,hg38}.comp.agp input/test.{hg19,hg38}.chrom.sizes output/$@.psl
	diff -u expected/$@.psl output/$@.psl

testHg38ToHg19: mkdirs
	${agpAlign} input/test.{hg38,hg19}.comp.agp input/test.{hg38,hg19}.chrom.sizes output/$@.psl
	diff -u expected/$@.psl output/$@.psl

clean::
	rm -rf output

mkdirs:
	@${MKDIR} output

