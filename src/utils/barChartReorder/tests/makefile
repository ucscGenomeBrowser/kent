kentSrc = ../../..
include ${kentSrc}/inc/common.mk

barChartReorder = ${DESTBINDIR}/barChartReorder

test: test1 test2 test3 test4 test5 test6

# reorder a bed6+5 file, rewriting the header
test1:
	@${MKDIR} -p output
	${barChartReorder} input/test.oldOrder input/test.newOrder input/test.bed \
	    output/$@.bed > output/$@.bars.txt
	diff expected/$@.bed output/$@.bed
	diff expected/$@.bars.txt output/$@.bars.txt

# bed6+3 with a trailing comma in expScores
test2:
	@${MKDIR} -p output
	${barChartReorder} input/test.oldOrder input/test.newOrder input/simple.bed \
	    output/$@.bed > /dev/null
	diff expected/$@.bed output/$@.bed

# reorder a .categories file to match
test3:
	@${MKDIR} -p output
	${barChartReorder} input/test.oldOrder input/test.newOrder input/test.bed \
	    output/$@.bed --categories input/test.categories \
	    --outCategories output/$@.categories > /dev/null
	diff expected/$@.categories output/$@.categories

# reordering twice returns the original file
test4:
	@${MKDIR} -p output
	${barChartReorder} input/test.oldOrder input/test.newOrder input/test.bed \
	    output/$@.once.bed > /dev/null
	${barChartReorder} input/test.newOrder input/test.oldOrder output/$@.once.bed \
	    output/$@.twice.bed > /dev/null
	diff input/test.bed output/$@.twice.bed

# order files naming different bars is an error
test5:
	@${MKDIR} -p output
	! ${barChartReorder} input/test.oldOrder input/bad.newOrder input/test.bed \
	    output/$@.bed 2> /dev/null

# a row with the wrong number of values is an error
test6:
	@${MKDIR} -p output
	! ${barChartReorder} input/test.oldOrder input/test.newOrder input/short.bed \
	    output/$@.bed 2> /dev/null

clean:
	rm -rf output
