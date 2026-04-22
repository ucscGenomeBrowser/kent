kentSrc = ../../../..
include ../../../../inc/common.mk

mafToBigMafSummary = ${DESTBINDIR}/mafToBigMafSummary

test: testdot testpipe clean

testdot: init
	${mafToBigMafSummary} hg38 input/testDot.maf output/testDot.bed
	diff expected/testDot.bed output/testDot.bed

testpipe: init
	${mafToBigMafSummary} hg38 input/testPipe.maf output/testPipe.bed
	diff expected/testPipe.bed output/testPipe.bed

init:
	${MKDIR} output

clean:
	rm -fr output
