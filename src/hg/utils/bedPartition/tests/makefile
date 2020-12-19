kentSrc = ../../../..
include ${kentSrc}/inc/common.mk

BPART = ${DESTBINDIR}/bedPartition
DIFF = diff

test: test1 test2

test1:
	@${MKDIR} -p output
	${BPART} -parallel=2 input/mrna1.bed output/$@.ranges.bed
	${DIFF} expected/$@.ranges.bed output/$@.ranges.bed

# merge partitions
test2:
	@${MKDIR} -p output
	${BPART} -parallel=2 -partSize=2 input/mrna1.bed output/$@.ranges.bed
	${DIFF} expected/$@.ranges.bed output/$@.ranges.bed


clean:
	rm -rf output
