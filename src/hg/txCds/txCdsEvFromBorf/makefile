kentSrc = ../../..
A = txCdsEvFromBorf
include ${kentSrc}/inc/userApp.mk

test::
	txCdsEvFromBorf in.borf in.fa out.tce
	diff expected.tce out.tce
