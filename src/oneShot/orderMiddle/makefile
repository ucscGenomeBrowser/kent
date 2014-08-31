kentSrc = ../..
A = orderMiddle
include $(kentSrc)/inc/userApp.mk

test::
	orderMiddle test.lst test.pair test.out
	diff test.expected test.out
