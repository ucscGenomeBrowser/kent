kentSrc = ../../../..
include ${kentSrc}/inc/common.mk

vcfToHgvs = ${DESTBINDIR}/vcfToHgvs

test:: PersonalisGroundTruth2017_agree PersonalisGroundTruth2017_disagree stopLoss
	${MAKE} clean

PersonalisGroundTruth2017_agree: mkdirs
	${vcfToHgvs} hg19 input/$@.vcf output/$@.tab
	sed -re 's/:/\t/g' output/$@.tab \
	| awk -F"\t" '(index($$4, $$8) == 1 && $$14 != $$5) {print;}' \
	  > output/$@_differences.tab
	diff -u expected/$@.tab output/$@.tab
	diff -u /dev/null output/$@_differences.tab

PersonalisGroundTruth2017_disagree: mkdirs
	${vcfToHgvs} hg19 input/$@.vcf output/$@.tab
	sed -re 's/:/\t/g' output/$@.tab \
	| awk -F"\t" '(index($$4, $$8) == 1 && $$14 == $$5) {print;}' \
	  > output/$@_matches.tab
	diff -u expected/$@.tab output/$@.tab
	diff -u /dev/null output/$@_matches.tab

stopLoss:
	${vcfToHgvs} hg38 input/$@.vcf output/$@.tab
	diff -u expected/$@.tab output/$@.tab

clean::
	rm -rf output

mkdirs:
	@${MKDIR} output
