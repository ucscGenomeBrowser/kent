include ../../../inc/common.mk

DIFF = diff -u
genePredToGtf = ${BINDIR}/genePredToGtf

test: basicTest utrTest stopBetweenTest honorTest

basicTest: mkout
	${genePredToGtf} -source=test file input.gp output/$@.gtf
	${DIFF} expected/$@.gtf output/$@.gtf

utrTest: mkout
	${genePredToGtf} -utr -source=test file input.gp output/$@.gtf
	${DIFF} expected/$@.gtf output/$@.gtf

stopBetweenTest: mkout
	${genePredToGtf} -stopBetweenCdsUtr -utr -source=test file input.gp output/$@.gtf
	${DIFF} expected/$@.gtf output/$@.gtf

honorTest: mkout
	${genePredToGtf} -honorCdsStat -utr -source=test file input.gp output/$@.gtf
	${DIFF} expected/$@.gtf output/$@.gtf

mkout:
	@${MKDIR} output

clean:
	rm -rf output
