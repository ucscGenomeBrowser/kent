kentSrc = ../../..
include ../../../inc/common.mk

pslDiff = ${DESTBINDIR}/pslDiff
DIFF = diff -u

test: countsTest countsNamesTest structDiffTest

countsTest: mkout
	${pslDiff} -details=output/$@.details input/single.psl input/base.psl >output/$@.diffs
	${DIFF} expected/$@.details output/$@.details
	${DIFF} expected/$@.diffs output/$@.diffs

countsNamesTest: mkout
	${pslDiff} -setNames -details=output/$@.details single input/single.psl base input/base.psl nonover input/nonover.psl >output/$@.diffs
	${DIFF} expected/$@.details output/$@.details
	${DIFF} expected/$@.diffs output/$@.diffs

structDiffTest: mkout
	${pslDiff} -details=output/$@.details input/set1a.psl input/set1b.psl input/set1c.psl >output/$@.diffs
	${DIFF} expected/$@.details output/$@.details
	${DIFF} expected/$@.diffs output/$@.diffs


mkout:
	@${MKDIR} output

clean:
	rm -rf output
