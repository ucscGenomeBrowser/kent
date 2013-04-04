include ../../../inc/common.mk

PROG = ../bedScore

test:: encodeTest
	echo tested all


# ENCODE narrow peak
encodeTest:
	@${MKDIR} output
	${PROG} -minScore=100 -col=7 input/encodeTfbsTest.bed output/encodeTfbsTest.bed
	@diff -w output/encodeTfbsTest.bed expected/

