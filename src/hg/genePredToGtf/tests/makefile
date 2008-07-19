include ../../../inc/common.mk

DIFF = diff -u
genePredToGtf = ${BINDIR}/genePredToGtf

# special cases:
#   - NM_021209 has a spliced start codon
#   - NM_001018001 marked 3' incomplete, positive strand
#   - D31736 marked 3' incomplete, negative strand.  CDS only contains
#     start codon.


test: \
	basicSimpleTest utrSimpleTest \
	basicTest utrTest honorTest

# tests using basic genePred
basicSimpleTest: mkout
	${genePredToGtf} -source=test file input/simple.gp output/$@.gtf
	${DIFF} expected/$@.gtf output/$@.gtf

utrSimpleTest: mkout
	${genePredToGtf} -utr -source=test file input/simple.gp output/$@.gtf
	${DIFF} expected/$@.gtf output/$@.gtf


# tests using extended genePred
basicTest: mkout
	${genePredToGtf} -source=test file input/extended.gp output/$@.gtf
	${DIFF} expected/$@.gtf output/$@.gtf

utrTest: mkout
	${genePredToGtf} -utr -source=test file input/extended.gp output/$@.gtf
	${DIFF} expected/$@.gtf output/$@.gtf

honorTest: mkout
	${genePredToGtf} -honorCdsStat -utr -source=test file input/extended.gp output/$@.gtf
	${DIFF} expected/$@.gtf output/$@.gtf

mkout:
	@${MKDIR} output

clean:
	rm -rf output
