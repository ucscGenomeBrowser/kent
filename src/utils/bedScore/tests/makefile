include ../../../inc/common.mk

PROG = ../bedScore

test: regTest miniTest encodeSmallTest encodeTest
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

# encodeTfbsTest is wgEncodeAwgTfbsUwWi38CtcfUniPk, with 3 rows removed (C and SQL rounding different)
encodeTest:
	@${MKDIR} output
	${PROG} -minScore=100 -col=7 input/encodeTfbsTest.bed output/encodeTfbsTest.bed
	@diff -w output/encodeTfbsTest.bed expected/

# use input file from V2 cluster track to compare scores in table
regTest:
	@${MKDIR} output
	${PROG} -method=reg -col=7 input/hsmmCtcfJan2011.narrowPeak output/encodeRegTest.bed
	@diff -w output/encodeRegTest.bed expected/

