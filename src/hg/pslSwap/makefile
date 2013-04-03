kentSrc = ../..
A = pslSwap
include $(kentSrc)/inc/userApp.mk

test:
	cd tests && ${MAKE} test

clean::
	cd tests && ${MAKE} clean

savebak:
	savebak pslSwap makefile *.c tests/makefile tests/input
