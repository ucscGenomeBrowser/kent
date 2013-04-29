kentSrc = ../../..
A = txGeneAccession
include ${kentSrc}/inc/userApp.mk

test::
	txGeneAccession inOld.bed in.id inNew.bed output.tab outOldToNew.tab -test
	diff expected.tab output.tab
