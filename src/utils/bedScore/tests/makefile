include ../../../inc/common.mk

PROG = ${BINDIR}/bedScore

test: miniTest encodeSmallTest encodeTest uniformTest asinhTest std2Test
	echo tested all

miniTest: outputDir
	${PROG} -minScore=100 input/miniTest.bed output/miniTest.bed
	@diff -w output/miniTest.bed expected/

# ENCODE narrow peak
encodeSmallTest: outputDir
	${PROG} -minScore=100 -col=7 input/encodeSmallTfbs.bed output/encodeSmallTfbs.bed
	@diff -w output/encodeSmallTfbs.bed expected/

# encodeTfbsTest is wgEncodeAwgTfbsUwWi38CtcfUniPk, with 3 rows removed (C and SQL rounding different)
encodeTest: outputDir
	${PROG} -minScore=100 -col=7 input/encodeTfbsTest.bed output/encodeTfbsTest.bed
	@diff -w output/encodeTfbsTest.bed expected/

# use input file from V2 cluster track to compare scores in table
regTest: outputDir
	${PROG} -method=reg -col=7 input/hsmmCtcfJan2011.narrowPeak output/encodeRegTest.bed
	@diff -w output/encodeRegTest.bed expected/

std2Test: outputDir
	${PROG} -method=std2 -col=7 input/hsmmCtcfJan2011.narrowPeak output/encodeStd2Test.bed
	@diff -w output/encodeStd2Test.bed expected/

uniformTest: outputDir
	${MKDIR} -p output/out
	@cat input/uniform1.bed input/uniform2.bed > input/uniform.bed
	${PROG} -col=7 input/uniform.bed expected/uniform.bed
	${PROG} -col=7 -uniform input/uniform1.bed input/uniform2.bed output/out
	@cat output/out/uniform1.bed output/out/uniform2.bed > output/uniform.bed
	@diff -w output/uniform.bed expected/

# test is from analysis hub, files scored by Steve Wilder (EBI) with minScore set to 370
# NOTE: rounding differs, so replacing expected/ with output from 
asinhTest:
	${PROG} -minScore=370 -method=asinh -col=7 input/broadGm12878Ctcf.jan2011.asinh.narrowPeak output/broadGm12878Ctcf.jan2011.asinh.narrowPeak
	@diff -w output/broadGm12878Ctcf.jan2011.asinh.narrowPeak expected/

clean::
	@rm -rf output

outputDir:
	@${MKDIR} -p output
