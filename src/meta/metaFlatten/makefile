kentSrc = ../..
A = metaFlatten
include $(kentSrc)/inc/userApp.mk
L += -lm

test::
	$A -indent=2 -withParent test.in test.out
	diff expected.out test.out

clean::
	rm -f test.out
