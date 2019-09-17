kentSrc = ../../..
A = hgNearTest
include $(kentSrc)/inc/userApp.mk

test::
	hgNearTest hgwdev-${USER}.soe.ucsc.edu/cgi-bin/hgNear org=Yeast repeat=1 test.log

clean::
	rm -f test.log
