kentSrc = ../..
A = tagStormReformat
include $(kentSrc)/inc/userApp.mk

test::
	$A -tab test.in test.tab
	diff expected.tab test.tab
	$A -flatten test.in test.ra
	diff expected.ra test.ra
	$A -tab -idTag=file test.in test.files
	diff expected.files test.files

clean::
	rm -f test.tab test.ra test.files
