kentSrc = ../../../..
A = hgGtex
include ${kentSrc}/inc/userApp.mk

test::
	${MKDIR} output
	hgGtex -tab=output -noLoad gtexTest input/small.gct input/samples.txt
	diff -r expected output
	rm -fr output

clean::
	rm -fr output
