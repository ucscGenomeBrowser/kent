include ../../../inc/common.mk

TEST_ALIGNS = close closeDnaRC closeMrnaDnaRC closeMrnaRC

all:

test: ${TEST_ALIGNS:%=%.trans-test} ${TEST_ALIGNS:%=%.untrans-test}


# test producing translated-format alignmenst
%.trans-test: mkdirs
	../spideyToPsl input/$*.aln output/$*.trans-test.psl
	diff -u expected/$*.trans-test.psl output/$*.trans-test.psl

# test producing untranslated-format alignmenst
%.untrans-test: mkdirs
	../spideyToPsl -untranslated input/$*.aln output/$*.untrans-test.psl
	diff -u expected/$*.untrans-test.psl output/$*.untrans-test.psl

clean::
	rm -rf output

mkdirs:
	@mkdir -p output

# format output for manual verification
format:
	pslPretty output/close.trans-test.psl input/dnaClose.fa input/mrnaClose.fa output/close.trans-test.ppsl
	pslPretty output/close.untrans-test.psl input/dnaClose.fa input/mrnaClose.fa output/close.untrans-test.ppsl
	pslPretty output/closeDnaRC.trans-test.psl input/dnaCloseRC.fa input/mrnaClose.fa output/closeDnaRC.trans-test.ppsl
	pslPretty output/closeDnaRC.untrans-test.psl input/dnaCloseRC.fa input/mrnaClose.fa output/closeDnaRC.untrans-test.ppsl
	pslPretty output/closeMrnaRC.trans-test.psl input/dnaClose.fa input/mrnaCloseRC.fa output/closeMrnaRC.trans-test.ppsl
	pslPretty output/closeMrnaRC.untrans-test.psl input/dnaClose.fa input/mrnaCloseRC.fa output/closeMrnaRC.untrans-test.ppsl
	pslPretty output/closeMrnaDnaRC.trans-test.psl input/dnaCloseRC.fa input/mrnaCloseRC.fa output/closeMrnaDnaRC.trans-test.ppsl
	pslPretty output/closeMrnaDnaRC.untrans-test.psl input/dnaCloseRC.fa input/mrnaCloseRC.fa output/closeMrnaDnaRC.untrans-test.ppsl

