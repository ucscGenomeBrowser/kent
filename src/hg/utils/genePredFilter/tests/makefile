include ../../../../inc/common.mk

genePredFilter = ${DESTBINDIR}/genePredFilter

test:: gencodeHackTest gencodeHackDbTest

gencodeHackTest: mkout
	${genePredFilter} input/gencodeHack.gp output/$@.gp
	diff expected/$@.gp output/$@.gp

gencodeHackDbTest: mkout
	${genePredFilter} -db=hg38 input/gencodeHack.gp output/$@.gp
	diff expected/$@.gp output/$@.gp


mkout:
	@mkdir -p output

clean::
	rm -rf output
