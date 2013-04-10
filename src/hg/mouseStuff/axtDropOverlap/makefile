kentSrc = ../../..
A = axtDropOverlap
include ${kentSrc}/inc/userApp.mk

test::	test1 test2
%:	input/%.axt
	${MKDIR} output
	axtDropOverlap input/$@.axt input/H input/H output/$@.axt
	diff output/$@.axt expected/$@.axt
	rm output/$@.axt

clean::
	rm -fr output
