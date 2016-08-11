kentSrc = ../../..
include ../../../inc/common.mk

PROG = tagStormDeleteTags

test: oneTest
	echo tested all

oneTest:
	mkdir -p output
	${PROG} input/in.tags input/delete.tags output/out.tags
	diff output/out.tags expected/out.tags

clean:
	@rm -rf output

