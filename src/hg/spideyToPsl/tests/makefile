kentSrc = ../../..
include ../../../inc/common.mk

spideyToPsl = ${DESTBINDIR}/spideyToPsl
pslCheck = ${DESTBINDIR}/pslCheck
pslPretty = ${DESTBINDIR}/pslPretty

ALIGN_TESTS = close closeDnaRC closeMrnaDnaRC closeMrnaRC zeroBlk
WARN_TESTS = overlap

all:

test: 	${ALIGN_TESTS:%=%.trans-test} \
	${ALIGN_TESTS:%=%.untrans-test} \
	${WARN_TESTS:%=%.warn}

# test producing translated-format alignmenst
%.trans-test: mkdirs
	${spideyToPsl} input/$*.aln output/$*.trans-test.psl
	${pslCheck} -verbose=0 output/$*.trans-test.psl
	diff -u expected/$*.trans-test.psl output/$*.trans-test.psl

# test producing untranslated-format alignmenst
%.untrans-test: mkdirs
	${spideyToPsl} -untranslated input/$*.aln output/$*.untrans-test.psl
	${pslCheck} -verbose=0 output/$*.untrans-test.psl
	diff -u expected/$*.untrans-test.psl output/$*.untrans-test.psl

# tests that produce warnings
%.warn: mkdirs
	${spideyToPsl} input/$*.aln output/$*.warn.psl > output/$*.warn.out 2>&1
	${pslCheck} -verbose=0 output/$*.warn.psl
	diff -u expected/$*.warn.psl output/$*.warn.psl
	diff -u expected/$*.warn.out output/$*.warn.out

# test producing untranslated-format alignmenst

clean::
	rm -rf output

mkdirs:
	@${MKDIR}p output

# make alignments
mkalns:
	spidey -m input/close.mrna.fa -i input/close.dna.fa -o input/close.aln
	spidey -m input/close.mrnaRC.fa -i input/close.dna.fa -o input/closeMrnaRC.aln
	spidey -m input/close.mrna.fa -i input/close.dnaRC.fa -o input/closeDnaRC.aln
	spidey -m input/close.mrnaRC.fa -i input/close.dnaRC.fa -o input/closeMrnaDnaRC.aln
	spidey -m input/overlap.mrna.fa -i input/overlap.dna.fa -o input/overlap.aln
	spidey -m input/zeroBlk.mrna.fa -i input/zeroBlk.dna.fa -o input/zeroBlk.aln


# format output for manual verification
format:
	${pslPretty} output/close.trans-test.psl input/close.dna.fa input/close.mrna.fa output/close.trans-test.ppsl
	${pslPretty} output/close.untrans-test.psl input/close.dna.fa input/close.mrna.fa output/close.untrans-test.ppsl
	${pslPretty} output/closeDnaRC.trans-test.psl input/close.dnaRC.fa input/close.mrna.fa output/closeDnaRC.trans-test.ppsl
	${pslPretty} output/closeDnaRC.untrans-test.psl input/close.dnaRC.fa input/close.mrna.fa output/closeDnaRC.untrans-test.ppsl
	${pslPretty} output/closeMrnaRC.trans-test.psl input/close.dna.fa input/close.mrnaRC.fa output/closeMrnaRC.trans-test.ppsl
	${pslPretty} output/closeMrnaRC.untrans-test.psl input/close.dna.fa input/close.mrnaRC.fa output/closeMrnaRC.untrans-test.ppsl
	${pslPretty} output/closeMrnaDnaRC.trans-test.psl input/close.dnaRC.fa input/close.mrnaRC.fa output/closeMrnaDnaRC.trans-test.ppsl
	${pslPretty} output/closeMrnaDnaRC.untrans-test.psl input/close.dnaRC.fa input/close.mrnaRC.fa output/closeMrnaDnaRC.untrans-test.ppsl

