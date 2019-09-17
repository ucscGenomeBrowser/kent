kentSrc = ../../../..
include ../../../../inc/common.mk

genePredFilter = ${DESTBINDIR}/genePredFilter

# -verbose=0 used to kept automated test from complaining

test:: gencodeHackTest gencodeHackDbTest
	${MAKE} clean

gencodeHackTest: mkout
	${genePredFilter} -verbose=0 input/gencodeHack.gp output/$@.gp
	diff expected/$@.gp output/$@.gp

gencodeHackDbTest: mkout
	${genePredFilter} -db=hg38 -verbose=0 input/gencodeHack.gp output/$@.gp
	diff expected/$@.gp output/$@.gp


mkout:
	@mkdir -p output

clean::
	rm -rf output
