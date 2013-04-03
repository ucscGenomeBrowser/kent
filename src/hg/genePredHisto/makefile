kentSrc = ../..
A = genePredHisto
include $(kentSrc)/inc/userApp.mk

test:
	cd tests && ${MAKE} test
	rm -fr tests/output

clean::
	rm -fr tests/output
