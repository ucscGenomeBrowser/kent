kentSrc = ../../..
include ../../../inc/common.mk

vcfFilter=${DESTBINDIR}/vcfFilter

test: testRenameGt testRenameNoGt testMinAc1Gt testMinAc1NoGt testMinAc2RenameGt \
	testMinAc1GtExclude

testRenameGt: mkdir
	${vcfFilter} -rename input/subset.vcf > output/$@.vcf
	diff -u expected/$@.vcf output/$@.vcf

testRenameNoGt: mkdir
	cut -f 1-8 input/subset.vcf | ${vcfFilter} -rename stdin > output/$@.vcf
	diff -u expected/$@.vcf output/$@.vcf

testMinAc1Gt: mkdir
	${vcfFilter} -minAc=1 input/subset.vcf > output/$@.vcf
	diff -u expected/$@.vcf output/$@.vcf

testMinAc1NoGt: mkdir
	cut -f 1-8 input/subset.vcf | ${vcfFilter} -minAc=1 stdin > output/$@.vcf
	diff -u expected/$@.vcf output/$@.vcf

testMinAc2RenameGt: mkdir
	${vcfFilter} -minAc=2 -rename input/subset.vcf > output/$@.vcf
	diff -u expected/$@.vcf output/$@.vcf

testMinAc1GtExclude: mkdir
	${vcfFilter} -minAc=1 input/subset.vcf -excludeVcf=input/exclude.vcf > output/$@.vcf
	diff -u expected/$@.vcf output/$@.vcf

clean::
	rm -rf output

mkdir:
	${MKDIR} -p output
