include ../../../inc/common.mk

BASEMASK = ../baseMask
BASEMASKTOBED = ../baseMaskToBed
BEDTOBASEMASK = ../bedToBaseMask
TRACKTOBASEMASK = ../trackToBaseMask
DIFF = diff 

test:   bedToBaseMaskTests \
	baseMaskToBedTests \
	trackToBaseMaskTests \
	baseMaskTests

bedToBaseMaskTests: mkout
	${BEDTOBASEMASK} test1.bed output/test1.bama
	${BEDTOBASEMASK} test2.bed output/test2.bama
	${BEDTOBASEMASK} test1ranges.bed output/test1ranges.bama
	${BEDTOBASEMASK} test2ranges.bed output/test2ranges.bama
	${BEDTOBASEMASK} test1or2ranges.bed output/test1or2ranges.bama
	${DIFF} --brief test1.bama     output/test1.bama
	${DIFF} --brief test2.bama     output/test2.bama
	${DIFF} --brief output/test1.bama output/test1ranges.bama
	${DIFF} --brief output/test2.bama output/test2ranges.bama
	${DIFF} --brief test1or2ranges.bama output/test1or2ranges.bama

baseMaskToBedTests: mkout
	${BASEMASKTOBED} output/test1.bama output/test1.bed
	${BASEMASKTOBED} output/test2.bama output/test2.bed
	${BASEMASKTOBED} output/test1ranges.bama output/test1ranges.bed
	${BASEMASKTOBED} output/test2ranges.bama output/test2ranges.bed
	${BASEMASKTOBED} output/test1or2ranges.bama output/test1or2ranges.bed
	${DIFF} --brief test1ranges.bed output/test1.bed
	${DIFF} --brief test2ranges.bed output/test2.bed
	${DIFF} --brief test1ranges.bed output/test1ranges.bed
	${DIFF} --brief test2ranges.bed output/test2ranges.bed
	${DIFF} --brief test1or2ranges.bed output/test1or2ranges.bed

trackToBaseMaskTests: mkout
#	${TRACKTOBASEMASK} 

baseMaskTests: mkout bedToBaseMaskTests baseMaskToBedTests
	${BASEMASK} -or output/test1.bama output/test2.bama output/test1or2.bama
	${BASEMASK} -or output/test1ranges.bama output/test2ranges.bama output/test1or2ranges.bama2
	${BASEMASKTOBED} output/test1or2.bama output/test1or2.bed2
	${BASEMASKTOBED} output/test1or2ranges.bama2 output/test1or2ranges.bed2
	${DIFF} --brief output/test1or2.bed2 test1or2ranges.bed
	${DIFF} --brief output/test1or2.bama test1or2ranges.bama
	${DIFF} --brief test1or2ranges.bed output/test1or2ranges.bed2
	${DIFF} --brief test1or2ranges.bama output/test1or2ranges.bama2

mkout:
	@${MKDIR} output

clean:
	rm -rf output

