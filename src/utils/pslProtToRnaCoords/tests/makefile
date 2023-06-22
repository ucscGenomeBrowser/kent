
kentSrc = ../../..
include ${kentSrc}/inc/common.mk

pslProtToRnaCoords = ${DESTBINDIR}/pslProtToRnaCoords
blastToPsl = ${DESTBINDIR}/blastToPsl
pslCheck = ${DESTBINDIR}/pslCheck

pslMapTestIn = ../../pslMap/tests/input

all:

test: kgProtTest spAnnotTest spIsoCanonTest spGencodeTest

kgProtTest: mkdirs
	${blastToPsl} ${pslMapTestIn}/kgProtMRna.blast output/kgProtMRna.psl
	${pslProtToRnaCoords} output/kgProtMRna.psl output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl

spAnnotTest: mkdirs
	${pslProtToRnaCoords} input/swissprotAnnotsToProt.psl output/$@.psl
	${pslCheck} -verbose=0  output/$@.psl
	diff -u expected/$@.psl output/$@.psl

spIsoCanonTest: mkdirs
	${pslProtToRnaCoords} input/swissprotIsoCanon.psl output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl

# this tests fixing overlapping blocks
spGencodeTest: mkdirs
	${pslProtToRnaCoords} input/swissprotGencode.psl output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl

clean::
	rm -rf output

mkdirs:
	@${MKDIR} output

