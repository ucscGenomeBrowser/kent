kentSrc = ../..
A = pslPartition
include $(kentSrc)/inc/userApp.mk

test:: compile
	cd tests && ${MAKE} test
	rm -fr tests/output

clean::
	rm -fr tests/output
