kentSrc = ../../..
include ../../../inc/common.mk

PROG = bedScore

test: miniTest encodeSmallTest encodeTest uniformTest asinhTest std2Test regTest
	echo tested all

miniTest: outputDir
	${PROG} -minScore=100 input/miniTest.bed output/miniTest.bed
	@diff -w output/miniTest.bed expected/

# ENCODE narrow peak
encodeSmallTest: outputDir
	${PROG} -minScore=100 -col=7 input/encodeSmallTfbs.bed output/encodeSmallTfbs.bed
	@diff -w output/encodeSmallTfbs.bed expected/

# encodeTfbsTest is wgEncodeAwgTfbsUwWi38CtcfUniPk, truncated to 1000 lines
encodeTest: outputDir
	${PROG} -minScore=100 -col=7 input/encodeTfbsTest.bed output/encodeTfbsTest.bed
	@diff -w output/encodeTfbsTest.bed expected/

# use input file from V2 cluster track (truncated to 1000 lines) to compare scores in table
regTest: outputDir
	${PROG} -method=reg -col=7 input/hsmmCtcfJan2011.narrowPeak output/regTest.narrowPeak
	@diff -w output/regTest.narrowPeak expected/

std2Test: outputDir
	${PROG} -method=std2 -col=7 input/hsmmCtcfJan2011.narrowPeak output/std2Test.narrowPeak
	@diff -w output/std2Test.narrowPeak expected/

uniformTest: outputDir
	${MKDIR} -p output/out
	${PROG} -col=7 -uniform input/uniform1.bed input/uniform2.bed output/out
	@cat output/out/uniform1.bed output/out/uniform2.bed > output/uniform.bed
	@diff -w output/uniform.bed expected/

# test is from analysis hub (truncated to 1000 lines), files scored by Steve Wilder (EBI) 
# with minScore set to 370
# NOTE: rounding differs, so replacing expected/ with output from 
asinhTest:
	${PROG} -minScore=370 -method=asinh -col=7 input/asinhTest.narrowPeak output/asinhTest.narrowPeak
	@diff -w output/asinhTest.narrowPeak expected/

clean::
	@rm -rf output

outputDir:
	@${MKDIR} -p output
