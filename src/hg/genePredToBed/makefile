kentSrc = ../..
A = genePredToBed
include $(kentSrc)/inc/userApp.mk

test::
	genePredToBed test.genePred test.bed
	diff expected.bed test.bed
	rm -f test.bed

clean::
	rm -f test.bed
