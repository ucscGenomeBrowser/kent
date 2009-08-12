include ../../../../inc/common.mk

gff3ToGenePred = ${BINDIR}/gff3ToGenePred

all::

test:: geneMRnaTest noGeneMRnaTest geneMRnaHonorTest


geneMRnaTest: mkout
	${gff3ToGenePred} input/geneMRna.gff3 output/$@.gp
	diff expected/$@.gp output/$@.gp

noGeneMRnaTest: mkout
	${gff3ToGenePred} input/noGeneMRna.gff3 output/$@.gp
	diff expected/$@.gp output/$@.gp

geneMRnaHonorTest: mkout
	${gff3ToGenePred} -honorStartStopCodons input/geneMRna.gff3 output/$@.gp
	diff expected/$@.gp output/$@.gp


mkout:
	@mkdir -p output

clean::
	rm -rf output
