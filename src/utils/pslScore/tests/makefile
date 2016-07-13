kentSrc = ../../..
include ../../../inc/common.mk

PROG = pslScore

test:
	pslScore test.psl > testOut.tab
	@diff expectOut.tab testOut.tab
	../pslScore.pl test.psl 2> /dev/null > testOutPl.tab
	@diff expectOut.tab testOutPl.tab

clean:
	rm -f testOut.tab testOutPl.tab
