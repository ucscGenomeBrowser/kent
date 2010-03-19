include ../../../../inc/common.mk

gff3ToGenePred = ${BINDIR}/gff3ToGenePred

all::

test:: geneMRnaTest noGeneMRnaTest geneMRnaHonorTest multCdsOutOfExonTest


geneMRnaTest: mkout
	${gff3ToGenePred} input/geneMRna.gff3 output/$@.gp
	diff expected/$@.gp output/$@.gp

noGeneMRnaTest: mkout
	${gff3ToGenePred} input/noGeneMRna.gff3 output/$@.gp
	diff expected/$@.gp output/$@.gp

geneMRnaHonorTest: mkout
	${gff3ToGenePred} -honorStartStopCodons input/geneMRna.gff3 output/$@.gp
	diff expected/$@.gp output/$@.gp

# was only reporting the first error
multCdsOutOfExonTest: mkout
	if ! ${gff3ToGenePred} input/multCdsOutOfExon.gff3 /dev/null >output/$@.out 2>&1 ; then true ; else echo "Error: comand should have failed" >&2; false ; fi
	diff expected/$@.out output/$@.out

mkout:
	@mkdir -p output

clean::
	rm -rf output
