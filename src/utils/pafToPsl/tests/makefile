kentSrc = ../../..
A = ${DESTBINDIR}/pafToPsl
include ../../../inc/common.mk

test: hg38ChrMt 

hg38ChrMt: outputDir
	${A} input/$@.paf output/$@.psl
	diff expected/$@.psl output/$@.psl

outputDir:
	@${MKDIR} -p output

clean:
	rm -fr output
