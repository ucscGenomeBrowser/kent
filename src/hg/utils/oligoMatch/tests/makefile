kentSrc = ../../../..
include ../../../../inc/common.mk

oligoMatch = ${DESTBINDIR}/oligoMatch

all:

test: basicTest

basicTest: mkdirs
	${oligoMatch} input/oligos.fa input/sequence.fa output/$@.bed
	diff -u expected/$@.bed output/$@.bed

clean::
	rm -rf output

mkdirs:
	@${MKDIR} output
