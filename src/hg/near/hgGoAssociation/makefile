kentSrc = ../../..
A = hgGoAssociation
include $(kentSrc)/inc/userApp.mk

test::
	hgGoAssociation go goaPart input/test1.goa -tab=output -noLoad -limit=500
	diff -x CVS -r expected output
	rm -fr output

clean::
	rm -fr output
