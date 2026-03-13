kentSrc = ../../..
A = ${DESTBINDIR}/faSize
include ../../../inc/common.mk

test: s1 s2 s1_s2 s1_2bit s2_2bit s1_s2_2bit s1AndS2 s1AndS2_2bit allThree allThree_2bit detailed detailed_2bit tab tab_2bit veryDetailed veryDetailed_2bit

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

detailed: outputDir
	${A} -detailed input/s1.s2.fa.gz > output/s1.s2.detailed.txt
	diff expected/s1.s2.detailed.txt output/s1.s2.detailed.txt

tab: outputDir
	${A} -tab input/s1.s2.fa.gz > output/s1.s2.tab.txt
	diff expected/s1.s2.tab.txt output/s1.s2.tab.txt

veryDetailed: outputDir
	${A} -veryDetailed input/s1.s2.fa.gz > output/s1.s2.veryDetailed.txt
	diff expected/s1.s2.veryDetailed.txt output/s1.s2.veryDetailed.txt

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

detailed_2bit: outputDir
	${A} -detailed input/s1.s2.2bit > output/s1.s2_2bit.detailed.txt
	diff expected/s1.s2.detailed.txt output/s1.s2_2bit.detailed.txt

tab_2bit: outputDir
	${A} -tab input/s1.s2.2bit > output/s1.s2_2bit.tab.txt
	diff expected/s1.s2.tab.txt output/s1.s2_2bit.tab.txt

veryDetailed_2bit: outputDir
	${A} -veryDetailed input/s1.s2.2bit > output/s1.s2_2bit.veryDetailed.txt
	diff expected/s1.s2.veryDetailed.txt output/s1.s2_2bit.veryDetailed.txt

outputDir:
	@${MKDIR} -p output

clean:
	rm -fr output
