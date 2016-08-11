kentSrc = ../../..
include ../../../inc/common.mk

test:
	tagStormInfo test.in > temp.out
	diff temp.out expected.out
	rm temp.out

clean:
	@rm -rf temp.out

