include ../../../inc/common.mk

PROG = ../bedScore

test: miniTest encodeSmallTest encodeTest
	echo tested all

miniTest:
	@${MKDIR} output
	${PROG} -minScore=100 input/miniTest.bed output/miniTest.bed
	@diff -w output/miniTest.bed expected/

# ENCODE narrow peak
encodeSmallTest:
	@${MKDIR} output
	${PROG} -minScore=100 -col=7 input/encodeSmallTfbs.bed output/encodeSmallTfbs.bed
	@diff -w output/encodeSmallTfbs.bed expected/

encodeTest:
	@${MKDIR} output
	${PROG} -minScore=100 -col=7 input/encodeTfbsTest.bed output/encodeTfbsTest.bed
	@diff -w output/encodeTfbsTest.bed expected/

