kentSrc = ../../..
A = hgMedianMicroarray
include ${kentSrc}/inc/userApp.mk

test::
	${MKDIR} output
	hgMedianMicroarray ce1 kimLifeCycle kimWormLifeCycle kimMed.ra kimLifeCycleMedian kimWormLifeCycleMedian -tab=output -noLoad -limit=500
	diff -x CVS -r expected output
	rm -fr output

clean::
	rm -fr output
