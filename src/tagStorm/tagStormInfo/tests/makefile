kentSrc = ../../..
include ../../../inc/common.mk

test:
	tagStormInfo test.in > temp.out
	diff temp.out expected.out
	rm temp.out
	tagStormInfo test.in -counts > temp.out
	diff temp.out expected.counts
	rm temp.out
	tagStormInfo test.in -vals=3 > temp.out
	diff temp.out expected.vals
	rm temp.out
	tagStormInfo test.in -schema > temp.out
	diff temp.out expected.schema
	rm temp.out

clean:
	@rm -rf temp.out

