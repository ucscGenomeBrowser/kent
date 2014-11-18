kentSrc = ../..
A = genePredHisto
include $(kentSrc)/inc/userApp.mk

test:: compile
	cd tests && ${MAKE} test
	rm -fr tests/output $A

clean::
	rm -fr tests/output
