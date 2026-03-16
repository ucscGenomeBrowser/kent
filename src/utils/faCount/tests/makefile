kentSrc = ../../..
A = ${DESTBINDIR}/faCount
include ../../../inc/common.mk

# faCount - count base statistics and CpGs in FA files.
# usage:
#    faCount file(s).fa
#      -summary  show only summary statistics
#      -dinuc    include statistics on dinucletoide frequencies
#      -strands  count bases on both strands

test: s1 s2 s1_s2 s1_2bit s2_2bit s1_s2_2bit s1AndS2 s1AndS2_2bit \
    allThree allThree_2bit summary summary_2bit dinuc dinuc_2bit \
    strands strands_2bit mixed mixedDinuc mixedSummary mixedStrands

s1: outputDir
	${A} input/s1.fa.gz > output/s1.txt
	diff expected/s1.txt output/s1.txt

s2: outputDir
	${A} input/s2.fa.gz > output/s2.txt
	diff expected/s2.txt output/s2.txt

s1_s2: outputDir
	${A} input/s1.s2.fa.gz > output/s1.s2.txt
	diff expected/s1.s2.txt output/s1.s2.txt

s1AndS2: outputDir
	${A} input/s1.fa.gz input/s2.fa.gz > output/s1Ands2.txt
	diff expected/s1Ands2.txt output/s1Ands2.txt

allThree: outputDir
	${A} input/s1.fa.gz input/s2.fa.gz input/s1.s2.fa.gz > output/allThree.txt
	diff expected/allThree.txt output/allThree.txt

summary: outputDir
	${A} -summary input/s1.s2.fa.gz > output/s1.s2.summary.txt
	diff expected/s1.s2.summary.txt output/s1.s2.summary.txt

dinuc: outputDir
	${A} -dinuc input/s1.s2.fa.gz > output/s1.s2.dinuc.txt
	diff expected/s1.s2.dinuc.txt output/s1.s2.dinuc.txt

strands: outputDir
	${A} -strands input/s1.s2.fa.gz > output/s1.s2.strands.txt
	diff expected/s1.s2.strands.txt output/s1.s2.strands.txt

s1_2bit: outputDir
	${A} input/s1.2bit > output/s1_2bit.txt
	diff expected/s1.txt output/s1_2bit.txt

s2_2bit: outputDir
	${A} input/s2.2bit > output/s2_2bit.txt
	diff expected/s2.txt output/s2_2bit.txt

s1_s2_2bit: outputDir
	${A} input/s1.s2.2bit > output/s1.s2_2bit.txt
	diff expected/s1.s2.txt output/s1.s2_2bit.txt

s1AndS2_2bit: outputDir
	${A} input/s1.2bit input/s2.2bit > output/s1Ands2_2bit.txt
	diff expected/s1Ands2.txt output/s1Ands2_2bit.txt

allThree_2bit: outputDir
	${A} input/s1.2bit input/s2.2bit input/s1.s2.2bit > output/allThree_2bit.txt
	diff expected/allThree.txt output/allThree_2bit.txt

summary_2bit: outputDir
	${A} -summary input/s1.s2.2bit > output/s1.s2_2bit.summary.txt
	diff expected/s1.s2.summary.txt output/s1.s2_2bit.summary.txt

dinuc_2bit: outputDir
	${A} -dinuc input/s1.s2.2bit > output/s1.s2_2bit.dinuc.txt
	diff expected/s1.s2.dinuc.txt output/s1.s2_2bit.dinuc.txt

strands_2bit: outputDir
	${A} -strands input/s1.s2.2bit > output/s1.s2_2bit.strands.txt
	diff expected/s1.s2.strands.txt output/s1.s2_2bit.strands.txt

mixed: outputDir
	${A} input/s1.2bit input/s1.fa.gz input/s2.2bit input/s2.fa.gz input/s1.s2.2bit input/s1.s2.fa.gz > output/mixed.txt
	diff expected/mixed.txt output/mixed.txt

mixedDinuc: outputDir
	${A} -dinuc input/s1.2bit input/s1.fa.gz input/s2.2bit input/s2.fa.gz input/s1.s2.2bit input/s1.s2.fa.gz > output/mixedDinuc.txt
	diff expected/mixedDinuc.txt output/mixedDinuc.txt

mixedSummary: outputDir
	${A} -summary input/s1.2bit input/s1.fa.gz input/s2.2bit input/s2.fa.gz input/s1.s2.2bit input/s1.s2.fa.gz > output/mixedSummary.txt
	diff expected/mixedSummary.txt output/mixedSummary.txt

mixedStrands: outputDir
	${A} -strands input/s1.2bit input/s1.fa.gz input/s2.2bit input/s2.fa.gz input/s1.s2.2bit input/s1.s2.fa.gz > output/mixedStrands.txt
	diff expected/mixedStrands.txt output/mixedStrands.txt

outputDir:
	@${MKDIR} -p output

clean:
	rm -fr output
