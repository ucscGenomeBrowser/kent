kentSrc = ../../../..
include ../../../../inc/common.mk

bigGenePredToGenePred = ${DESTBINDIR}/bigGenePredToGenePred
bedToBigBed = ${DESTBINDIR}/bedToBigBed
genePredCheck = ${DESTBINDIR}/genePredCheck

test: basicTest


basicTest: mkout
	${bedToBigBed} -type=bed12+8 -as=${kentSrc}/hg/lib/bigGenePred.as input/basic.bigGpIn input/hg38.sizes output/$@.biggp
	${bigGenePredToGenePred} output/$@.biggp output/$@.gp
	${genePredCheck} -chromSizes=input/hg38.sizes output/basicTest.gp 
	diff expected/$@.gp output/$@.gp

mkout:
	@${MKDIR} output

clean:
	rm -rf output
