kentSrc = ../../..
include ../../../inc/common.mk

A = tagStormDeleteTags

test:
	mkdir -p output
	${A} input/in.tags input/delete.tags output/out.tags
	diff output/out.tags expected/out.tags
	@rm -rf output

clean:
	@rm -rf output

