kentSrc = ../../..
include ../../../inc/common.mk

genePredSingleCover = ${DESTBINDIR}/genePredSingleCover

DIFF = diff -u

test:  simpleTest

simpleTest: mkout
	${genePredSingleCover} input/simple.gp output/$@.gp
	${DIFF} expected/$@.gp output/$@.gp

mkout:
	@${MKDIR} output

clean:
	rm -rf output
