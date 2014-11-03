kentSrc = ../..
A = genePredSingleCover
extraObjects = geneScore.o
include $(kentSrc)/inc/userApp.mk

test:: compile
	cd tests && ${MAKE} test
	rm -fr tests/output $A

savebak:
	savebak genePredSingleCover  *.[ch] makefile tests/makefile tests/input

clean::
	rm -fr tests/output
