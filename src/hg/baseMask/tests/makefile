include ../../../inc/common.mk

BASEMASK = ../baseMask -verbose=0 
BASEMASKTOBED = ../baseMaskToBed -verbose=0 
BEDTOBASEMASK = ../bedToBaseMask -verbose=0 
TRACKTOBASEMASK = ../trackToBaseMask -verbose=0 
FEATUREBITS = featureBits -countGaps
BEDSORT = bedSort
DIFF = diff 
TIME = time

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
	${TIME} ${FEATUREBITS} hg18 knownGene -bed=output/knownGene.fb.bed
	# takes a few minutes for the next few featureBits commands
	${TIME} ${FEATUREBITS} hg18 rmsk      -bed=output/rmsk.fb.bed
	${TIME} ${FEATUREBITS} hg18     knownGene rmsk -bed=output/knownGene_and_rmsk.fb.bed
	${TIME} ${FEATUREBITS} hg18 -or knownGene rmsk -bed=output/knownGene_or_rmsk.fb.bed
	${BEDSORT} output/knownGene.fb.bed          output/knownGene.fb.bed
	${BEDSORT} output/rmsk.fb.bed               output/rmsk.fb.bed
	${BEDSORT} output/knownGene_and_rmsk.fb.bed output/knownGene_and_rmsk.fb.bed
	${BEDSORT} output/knownGene_or_rmsk.fb.bed  output/knownGene_or_rmsk.fb.bed
	${TIME} ${TRACKTOBASEMASK} hg18 knownGene   output/knownGene.bama
	${TIME} ${BASEMASKTOBED}   -id -merge       output/knownGene.bama output/knownGene.bm.bed
	${DIFF} --brief output/knownGene.fb.bed output/knownGene.bm.bed
	${TIME} ${TRACKTOBASEMASK} hg18 rmsk       output/rmsk.bama
	${TIME} ${BASEMASKTOBED}   -id -merge      output/rmsk.bama output/rmsk.bm.bed
	${DIFF} --brief output/rmsk.fb.bed         output/rmsk.bm.bed
	${TIME} ${BASEMASK} -and output/knownGene.bama output/rmsk.bama    output/knownGene_and_rmsk.bama
	${TIME} ${BASEMASKTOBED} -id -merge output/knownGene_and_rmsk.bama output/knownGene_and_rmsk.bm.bed
	${DIFF} --brief output/knownGene_and_rmsk.fb.bed                   output/knownGene_and_rmsk.bm.bed
	${TIME} ${BASEMASK} -or  output/knownGene.bama output/rmsk.bama    output/knownGene_or_rmsk.bama
	${TIME} ${BASEMASKTOBED} -id -merge output/knownGene_or_rmsk.bama  output/knownGene_or_rmsk.bm.bed
	${DIFF} --brief output/knownGene_or_rmsk.fb.bed                    output/knownGene_or_rmsk.bm.bed

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

