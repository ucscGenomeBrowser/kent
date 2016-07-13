kentSrc = ../../../..
include ../../../../inc/common.mk

axtToPsl=${DESTBINDIR}/axtToPsl

all:

test: blockBug

# bug in pslFromAlign where the block count got confused
blockBug: mkdirs
	${axtToPsl} input/blockBug.axt input/blockBug.sizes input/blockBug.sizes output/$@.psl
	diff -u expected/$@.psl output/$@.psl

mkdirs:
	@${MKDIR} output

clean::
	rm -fr output
