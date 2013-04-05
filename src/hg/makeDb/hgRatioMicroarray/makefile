kentSrc = ../../..
A = hgRatioMicroarray
include ${kentSrc}/inc/userApp.mk

test::
	${MKDIR} output
	hgRatioMicroarray gnfHumanU95All gnfHumanU95AllRatio -clump=gnfClump.ra -noLoad -limit=100 -tab=output
	diff -x CVS -r expected output
	rm -fr output

clean::
	rm -fr output
