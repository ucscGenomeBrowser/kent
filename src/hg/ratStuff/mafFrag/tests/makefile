kentSrc = ../../../..
include ${kentSrc}/inc/common.mk

MAFFRAG = ${DESTDIR}${BINDIR}/mafFrag

all::

test:: basicTest revStrandTest outNameTest smallTest largerTest noDotsTest noDotsRevTest noDotsOutNameTest noDotsLargerTest
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

noDotsTest: mkdirs
	${MAFFRAG} hg38 multiz100way chr1 11000 11100 + -noDots output/$@.maf
	diff expected/noDots.maf output/$@.maf

noDotsRevTest: mkdirs
	${MAFFRAG} hg38 multiz100way chr1 11000 11100 - -noDots output/$@.maf
	diff expected/noDotsRev.maf output/$@.maf

noDotsOutNameTest: mkdirs
	${MAFFRAG} hg38 multiz100way chr1 11000 11100 + -noDots -outName=testSeq output/$@.maf
	diff expected/noDotsOutName.maf output/$@.maf

noDotsLargerTest: mkdirs
	${MAFFRAG} hg38 multiz100way chr1 100000 100500 + -noDots output/$@.maf
	diff expected/noDotsLarger.maf output/$@.maf

clean::
	rm -rf output
