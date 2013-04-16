kentSrc = ../../..
A = bedThickToCds
include $(kentSrc)/inc/userApp.mk

test::
	bedThickToCds in.bed out.cds
	diff expected.cds out.cds

clean::
	rm -f out.cds
