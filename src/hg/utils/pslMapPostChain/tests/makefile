kentSrc = ../../../..
include ${kentSrc}/inc/common.mk

pslMapPostChain = ${DESTBINDIR}/pslMapPostChain

all:

test: C57B6NJ-1504TransMapTest


C57B6NJ-1504TransMapTest: mkdirs
	${pslMapPostChain} input/C57B6NJ-1504.transMap-cases.psl output/$@.psl
	pslCheck output/$@.psl
	diff -u expected/$@.psl output/$@.psl

mkdirs:
	@mkdir -p output

clean:
	rm -rf output
