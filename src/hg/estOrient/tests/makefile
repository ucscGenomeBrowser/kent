kentSrc = ../../..
include ../../../inc/common.mk

ESTORIENT = ${DESTBINDIR}/estOrient
DIFF = diff -u

test: estFileTest

###
# tests that only use a file
###
estFileTest: mkout
	${ESTORIENT} -fileInput -info=output/$@.info -estOrientInfo=input/ests.polyinfo no input/ests.psl output/$@.psl
	${DIFF} expected/$@.psl output/$@.psl
	${DIFF} expected/$@.info output/$@.info

mkout:
	@${MKDIR} output

clean:
	rm -rf output
