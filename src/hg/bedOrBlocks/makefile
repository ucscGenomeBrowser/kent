kentSrc = ../..
A = bedOrBlocks
include $(kentSrc)/inc/userApp.mk

test::
	bedOrBlocks in.bed out.bed
	diff expected.bed out.bed

clean::
	rm -f out.bed
