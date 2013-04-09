kentSrc = ../..
A = weedLines
include $(kentSrc)/inc/userApp.mk
L += -lm

test::
	weedLines input.weed input.txt out.good -weeded=out.bad
	diff expected.good out.good
	diff expected.bad out.bad
	
clean::
	rm -f out.good out.bad
