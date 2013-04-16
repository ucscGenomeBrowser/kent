kentSrc = ../../..
A = mafCoverage
include ${kentSrc}/inc/userApp.mk

test::	test1 test2
%:	input/%.maf
	${MKDIR} output
	mafCoverage hg15 input/$@.maf > output/$@.txt
	diff output/$@.txt expected/$@.txt
	rm output/$@.txt

clean::
	rm -fr output
