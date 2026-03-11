kentSrc = ../../../..
include ${kentSrc}/inc/common.mk

MAFFRAG = ${DESTDIR}${BINDIR}/mafFrag

all::

test:: basicTest revStrandTest outNameTest smallTest largerTest
	@echo "All mafFrag tests passed."

mkdirs:
	@${MKDIR} output

basicTest: mkdirs
	${MAFFRAG} hg38 multiz100way chr1 11000 11100 + output/$@.maf
	diff expected/basic.maf output/$@.maf

revStrandTest: mkdirs
	${MAFFRAG} hg38 multiz100way chr1 11000 11100 - output/$@.maf
	diff expected/revStrand.maf output/$@.maf

outNameTest: mkdirs
	${MAFFRAG} hg38 multiz100way chr1 11000 11100 + -outName=testSeq output/$@.maf
	diff expected/outName.maf output/$@.maf

smallTest: mkdirs
	${MAFFRAG} hg38 multiz100way chr1 11000 11010 + output/$@.maf
	diff expected/small.maf output/$@.maf

largerTest: mkdirs
	${MAFFRAG} hg38 multiz100way chr1 100000 100500 + output/$@.maf
	diff expected/larger.maf output/$@.maf

clean::
	rm -rf output
