kentSrc = ../../..
include ../../../inc/common.mk

test:
	tagStormJoinTab meta test.tab test.tags temp.out
	diff temp.out expected.out
	rm temp.out

clean:
	@rm -rf temp.out

