kentSrc = ../../..
include ../../../inc/common.mk

# Run the copy in the tree rather than an installed one, so that a plain
# "make test" works without a preceding install and always exercises the
# code that is about to be committed.
reorder = ../barChartReorder

all:
	mkdir -p out
	@printf "\ntest1: reorder a bed6+5 file, rewriting the header\n"
	${reorder} in/test.oldOrder in/test.newOrder in/test.bed out/test.bed > out/bars.txt
	diff out/test.bed expected/test.bed
	diff out/bars.txt expected/bars.txt
	@printf "\ntest2: bed6+3 with a trailing comma in expScores\n"
	${reorder} in/test.oldOrder in/test.newOrder in/simple.bed out/simple.bed > /dev/null
	diff out/simple.bed expected/simple.bed
	@printf "\ntest3: reorder a .categories file to match\n"
	${reorder} in/test.oldOrder in/test.newOrder in/test.bed out/test.bed \
	    --categories in/test.categories --outCategories out/test.categories > /dev/null
	diff out/test.categories expected/test.categories
	@printf "\ntest4: reordering twice returns the original file\n"
	${reorder} in/test.oldOrder in/test.newOrder in/test.bed out/once.bed > /dev/null
	${reorder} in/test.newOrder in/test.oldOrder out/once.bed out/twice.bed > /dev/null
	diff out/twice.bed in/test.bed
	@printf "\ntest5: order files naming different bars is an error\n"
	! ${reorder} in/test.oldOrder in/bad.newOrder in/test.bed out/bad.bed 2> /dev/null
	@printf "\ntest6: a row with the wrong number of values is an error\n"
	! ${reorder} in/test.oldOrder in/test.newOrder in/short.bed out/bad.bed 2> /dev/null
	@printf "\nall barChartReorder tests passed\n"

test: all

clean:
	rm -rf out
