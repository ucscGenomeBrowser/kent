include ../../../../inc/common.mk
bedToPsl = ${BINDIR}/bedToPsl

all:

test: basicTest

basicTest: mkdirs
	${bedToPsl} input/chrom.sizes input/basic.bed output/$@.psl
	pslCheck -verbose=0 output/$@.psl
	diff -u expected/$@.psl output/$@.psl

clean::
	rm -rf output

mkdirs:
	@${MKDIR} output
