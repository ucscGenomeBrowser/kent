kentSrc = ../../..
A = txCdsOrtho
include ${kentSrc}/inc/userApp.mk

test::
	txCdsOrtho in.cds in.maf out.tab
	diff expected.tab out.tab
