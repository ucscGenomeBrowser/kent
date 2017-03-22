kentSrc = ../../../..
include ${kentSrc}/inc/common.mk

hgvsToVcf = ${DESTBINDIR}/hgvsToVcf

all:

test: clinVarChanges testShifting
	${MAKE} clean

clinVarChanges: mkdirs
	${hgvsToVcf} hg38 input/$@.txt output/$@.vcf
	diff -u expected/$@.vcf output/$@.vcf

testShifting: mkdirs
	${hgvsToVcf} hg38 input/$@.txt output/$@.vcf
	diff -u expected/$@.vcf output/$@.vcf

clean::
	rm -rf output

mkdirs:
	@${MKDIR} output

