kentSrc = ../../../..
include ${kentSrc}/inc/common.mk

BPART = ${DESTBINDIR}/bedPartition
DIFF = diff

test: test1 test2 test3

define diff_results
	${DIFF} expected/$@.ranges.bed output/$@.ranges.bed
	${DIFF} expected/$@.out output/$@.out
endef

# straight partations
test1:
	@${MKDIR} -p output
	${BPART} -parallel=2 input/mrna1.bed output/$@.ranges.bed >& output/$@.out
	${diff_results}

# merge adjacent partitions
test2:
	@${MKDIR} -p output
	${BPART} -parallel=2 -partMergeDist=10000 input/mrna1.bed output/$@.ranges.bed >& output/$@.out
	${diff_results}

# minimum reads per partition
test3:
	@${MKDIR} -p output
	${BPART} -parallel=2 -minPartitionItems=3 input/mrna1.bed output/$@.ranges.bed >& output/$@.out
	${diff_results}

clean:
	rm -rf output
