GBROOT = ../../..
include ${GBROOT}/common.mk

PROCESS = ${BINARCH}/gbProcess

all:

test: refseqTest

refseqTest:
	@${MKDIR} output
	${PROCESS} output/$@.fa output/$@.ra input/refseq.gbff

clean:
	rm -f output
