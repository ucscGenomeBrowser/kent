kentSrc = ../..
A = distributeRgbRainbow
include $(kentSrc)/inc/userApp.mk

test::
	$A test.lst test.pair test.out
	diff test.expected test.out
