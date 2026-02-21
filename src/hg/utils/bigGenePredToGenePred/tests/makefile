kentSrc = ../../../..
include ../../../../inc/common.mk

bigGenePredToGenePred = ${DESTBINDIR}/bigGenePredToGenePred
genePredToBigGenePred = ${DESTBINDIR}/genePredToBigGenePred
bedToBigBed = ${DESTBINDIR}/bedToBigBed
genePredCheck = ${DESTBINDIR}/genePredCheck

test: basicTest


basicTest: mkout
	${genePredToBigGenePred} input/basicTest.gp stdout | sort -k1,1 -k2,2n > output/basic.bigGpIn 
	${bedToBigBed} -tab -type=bed12+8 -as=${kentSrc}/hg/lib/bigGenePred.as output/basic.bigGpIn input/hg38.sizes output/$@.biggp
	${bigGenePredToGenePred} output/$@.biggp output/$@.gp
	${genePredCheck} -chromSizes=input/hg38.sizes output/basicTest.gp 
	diff expected/$@.gp output/$@.gp

mkout:
	@${MKDIR} output

clean:
	rm -rf output
