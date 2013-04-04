kentSrc = ../../..
A = hgLoadRnaFold
include $(kentSrc)/inc/userApp.mk

test::
	hgLoadRnaFold -tab=output -noLoad hg16 test input
	diff -x CVS -r expected output
	rm -fr output

clean::
	rm -fr output
