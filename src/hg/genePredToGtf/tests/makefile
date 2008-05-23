include ../../../inc/common.mk

DIFF = diff -u
genePredToGtf = ${BINDIR}/genePredToGtf

test: \
	basicSimpleTest utrSimpleTest stopBetweenSimpleTest honorSimpleTest \
	basicTest utrTest stopBetweenTest honorTest

# tests using basic genePred
basicSimpleTest: mkout
	${genePredToGtf} -source=test file input/simple.gp output/$@.gtf
	${DIFF} expected/$@.gtf output/$@.gtf

utrSimpleTest: mkout
	${genePredToGtf} -utr -source=test file input/simple.gp output/$@.gtf
	${DIFF} expected/$@.gtf output/$@.gtf

stopBetweenSimpleTest: mkout
	${genePredToGtf} -stopBetweenCdsUtr -utr -source=test file input/simple.gp output/$@.gtf
	${DIFF} expected/$@.gtf output/$@.gtf

honorSimpleTest: mkout
	${genePredToGtf} -honorCdsStat -utr -source=test file input/simple.gp output/$@.gtf
	${DIFF} expected/$@.gtf output/$@.gtf


# tests using extended genePred
basicTest: mkout
	${genePredToGtf} -source=test file input/extended.gp output/$@.gtf
	${DIFF} expected/$@.gtf output/$@.gtf

utrTest: mkout
	${genePredToGtf} -utr -source=test file input/extended.gp output/$@.gtf
	${DIFF} expected/$@.gtf output/$@.gtf

stopBetweenTest: mkout
	${genePredToGtf} -stopBetweenCdsUtr -utr -source=test file input/extended.gp output/$@.gtf
	${DIFF} expected/$@.gtf output/$@.gtf

honorTest: mkout
	${genePredToGtf} -honorCdsStat -utr -source=test file input/extended.gp output/$@.gtf
	${DIFF} expected/$@.gtf output/$@.gtf

mkout:
	@${MKDIR} output

clean:
	rm -rf output
