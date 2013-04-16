kentSrc = ../../..
A = orfCompare
include $(kentSrc)/inc/userApp.mk

test::
	orfCompare inA.orf inB.orf > out.txt
	diff expected.txt out.txt

clean::
	rm -f out.txt
