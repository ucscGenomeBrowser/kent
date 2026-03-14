kentSrc = ../../..
A = mafFrag
include ${kentSrc}/inc/userApp.mk

test::
	mafFrag hg16 rat_hmrg chr2 69065807 69073183 - test.maf
	@diff test.maf expected.maf

clean::
	rm -f test.maf
