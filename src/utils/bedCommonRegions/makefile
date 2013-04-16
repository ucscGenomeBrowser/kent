kentSrc = ../..
A = bedCommonRegions
include $(kentSrc)/inc/userApp.mk
L += -lm

test::
	bedCommonRegions tests/in/1.bed tests/in/2.bed tests/in/3.bed > tests/out.bed
	diff tests/expected.bed tests/out.bed

clean::
	rm -f tests/out.bed
