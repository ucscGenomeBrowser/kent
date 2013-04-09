kentSrc = ../../../..
A = hgGnfMicroarray
include ${kentSrc}/inc/userApp.mk

test::
	${MKDIR} output
	hgGnfMicroarray -tab=output -noLoad -limit=100 gnfHumanU95Exps gnfHumanU95All /projects/compbio/data/microarray/affyGnfHuman/data_public_U95
	diff -x CVS -r expected output
	rm -fr output

clean::
	rm -fr output
