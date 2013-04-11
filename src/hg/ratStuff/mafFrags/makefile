kentSrc = ../../..
A = mafFrags
include ${kentSrc}/inc/userApp.mk

test::
	${MKDIR} output
	mafFrags hg16 rat_hmrg input/hg16.bed output/hg16.maf -orgs=input/hg16.org
	diff -x CVS -r expected output
	rm -fr output

clean::
	rm -fr output
