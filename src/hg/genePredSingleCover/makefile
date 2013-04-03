kentSrc = ../..
A = genePredSingleCover
extraObjects = geneScore.o
include $(kentSrc)/inc/userApp.mk

test:
	cd tests && ${MAKE} test
	rm -fr tests/output

savebak:
	savebak genePredSingleCover  *.[ch] makefile tests/makefile tests/input

clean::
	rm -fr tests/output
