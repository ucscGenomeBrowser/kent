kentSrc = ../../../..
include ${kentSrc}/inc/common.mk

hgvsToVcf = ${DESTBINDIR}/hgvsToVcf

all:

test: clinVarChanges testShifting
	${MAKE} clean

clinVarChanges: mkdirs
	${hgvsToVcf} hg38 input/$@.txt output/$@.vcf 2>output/$@.err
	diff -u expected/$@.vcf output/$@.vcf
	diff -u expected/$@.err output/$@.err

testShifting: mkdirs
	${hgvsToVcf} hg38 input/$@.txt output/$@.vcf 2>output/$@.err
	diff -u expected/$@.vcf output/$@.vcf
	diff -u expected/$@.err output/$@.err

clean::
	rm -rf output

mkdirs:
	@${MKDIR} output

