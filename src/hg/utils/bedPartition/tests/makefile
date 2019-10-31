kentSrc = ../../../..
include ${kentSrc}/inc/common.mk

BPART = ${DESTBINDIR}/bedPartition
DIFF = diff

test: test1

test1:
	@${MKDIR} -p output
	${BPART} input/mrna1.bed output/$@.ranges.bed
	${DIFF} expected/$@.ranges.bed output/$@.ranges.bed


clean:
	rm -rf output
