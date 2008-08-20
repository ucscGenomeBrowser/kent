include ../../../inc/common.mk

BASEMASK = ../baseMask -verbose=0 
BASEMASKTOBED = ../baseMaskToBed -verbose=0 
BEDTOBASEMASK = ../bedToBaseMask -verbose=0 
TRACKTOBASEMASK = ../trackToBaseMask -verbose=0 
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
	${BEDTOBASEMASK} test1or2ranges.bed  output/test1or2ranges.bama
	${BEDTOBASEMASK} test1and2ranges.bed output/test1and2ranges.bama
	${DIFF} --brief test1.bama     output/test1.bama
	${DIFF} --brief test2.bama     output/test2.bama
	${DIFF} --brief output/test1.bama output/test1ranges.bama
	${DIFF} --brief output/test2.bama output/test2ranges.bama
	${DIFF} --brief test1or2ranges.bama  output/test1or2ranges.bama
	${DIFF} --brief test1and2ranges.bama output/test1and2ranges.bama

baseMaskToBedTests: mkout
	${BASEMASKTOBED} output/test1.bama output/test1.bed
	${BASEMASKTOBED} output/test2.bama output/test2.bed
	${BASEMASKTOBED} output/test1ranges.bama output/test1ranges.bed
	${BASEMASKTOBED} output/test2ranges.bama output/test2ranges.bed
	${BASEMASKTOBED} output/test1or2ranges.bama  output/test1or2ranges.bed
	${BASEMASKTOBED} output/test1and2ranges.bama output/test1and2ranges.bed
	${BASEMASKTOBED} -id -merge output/test1.bama           output/test1.idmerge.bed
	${BASEMASKTOBED} -id -merge output/test1or2ranges.bama  output/test1or2.idmerge.bed
	${BASEMASKTOBED} -id -merge output/test1and2ranges.bama output/test1and2.idmerge.bed
	${DIFF} --brief test1ranges.bed output/test1.bed
	${DIFF} --brief test2ranges.bed output/test2.bed
	${DIFF} --brief test1ranges.bed output/test1ranges.bed
	${DIFF} --brief test2ranges.bed output/test2ranges.bed
	${DIFF} --brief test1or2ranges.bed  output/test1or2ranges.bed
	${DIFF} --brief test1and2ranges.bed output/test1and2ranges.bed
	${DIFF} --brief test1.fb.bed     output/test1.idmerge.bed
	${DIFF} --brief test1or2.fb.bed  output/test1or2.idmerge.bed
	${DIFF} --brief test1and2.fb.bed output/test1and2.idmerge.bed

trackToBaseMaskTests: mkout
#	${TRACKTOBASEMASK} 

baseMaskTests: mkout bedToBaseMaskTests baseMaskToBedTests
	${BASEMASK} -or output/test1.bama        output/test2.bama 2>output/test1or2.stderr
	${BASEMASK} -or output/test1ranges.bama  output/test2ranges.bama 2>output/test1or2ranges.stderr
	${BASEMASK} -and output/test1.bama       output/test2.bama 2>output/test1and2.stderr
	${BASEMASK} -and output/test1ranges.bama output/test2ranges.bama 2>output/test1and2ranges.stderr
	${BASEMASK} -or -quiet output/test1.bama       output/test2.bama output/test1or2.bama
	${BASEMASK} -or -quiet output/test1ranges.bama output/test2ranges.bama output/test1or2ranges.bama2
	${BASEMASK} -and -quiet output/test1.bama       output/test2.bama output/test1and2.bama
	${BASEMASK} -and -quiet output/test1ranges.bama output/test2ranges.bama output/test1and2ranges.bama2
	${BASEMASKTOBED} output/test1or2.bama        output/test1or2.bed2
	${BASEMASKTOBED} output/test1or2ranges.bama2 output/test1or2ranges.bed2
	${BASEMASKTOBED} output/test1and2.bama        output/test1and2.bed2
	${BASEMASKTOBED} output/test1and2ranges.bama2 output/test1and2ranges.bed2
	${DIFF} test1or2.stderr  output/test1or2.stderr
	${DIFF} test1or2.stderr  output/test1or2ranges.stderr
	${DIFF} test1and2.stderr output/test1and2.stderr
	${DIFF} test1and2.stderr output/test1and2ranges.stderr
	${DIFF} --brief test1or2ranges.bama  output/test1or2.bama
	${DIFF} --brief test1or2ranges.bama  output/test1or2ranges.bama2
	${DIFF} --brief test1and2ranges.bama output/test1and2.bama 
	${DIFF} --brief test1and2ranges.bama output/test1and2ranges.bama2
	${DIFF} --brief test1or2ranges.bed   output/test1or2.bed2
	${DIFF} --brief test1or2ranges.bed   output/test1or2ranges.bed2
	${DIFF} --brief test1and2ranges.bed  output/test1and2.bed2
	${DIFF} --brief test1and2ranges.bed  output/test1and2ranges.bed2

mkout:
	@${MKDIR} output

clean:
	rm -rf output

