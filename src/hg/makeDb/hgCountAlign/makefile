kentSrc = ../../..
A = hgCountAlign
extraObjects = align.o slidingWin.o  winCounts.o
include ${kentSrc}/inc/userApp.mk

clean::
	rm -rf output

test:: test1 test2 test3 test4

test1:
	@${MKDIR} output
	${A} test.axt output/test1.tab
	diff expected/test1.tab output/test1.tab

test2:
	@${MKDIR} output
	${A} -winSize=200 -winSlide=50 test.axt output/test2.tab
	diff expected/test2.tab output/test2.tab

test3:
	@${MKDIR} output
	${A} -winSize=200 -winSlide=50 -fixedNumCounts=50 -countCoords \
	     test.axt output/test3.tab
	diff expected/test3.tab output/test3.tab

test4:
	@${MKDIR} output
	${A} -winSize=200 -winSlide=50 -selectBed=test-sel.bed \
	    test.axt output/test4.tab
	diff expected/test4.tab output/test4.tab
