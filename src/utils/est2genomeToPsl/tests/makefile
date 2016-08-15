kentSrc = ../../..
include ../../../inc/common.mk

est2genomeToPsl=${DESTBINDIR}/est2genomeToPsl

test:: basic

basic: mkout
	${est2genomeToPsl} input/basic.e2g input/basicSplicedEst.fa input/basicGenome.fa output/$@.psl
	pslCheck -verbose=0 output/$@.psl 
	diff expected/$@.psl output/$@.psl

mkout:
	${MKDIR} output

clean::
	rm -rf output
