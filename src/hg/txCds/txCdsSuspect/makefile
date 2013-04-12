kentSrc = ../../..
A = txCdsSuspect
include ${kentSrc}/inc/userApp.mk

test::
	txCdsSuspect in.bed in.txg in.cluster in.info out.suspect -niceProt=out.nice
	diff expected.suspect out.suspect
	diff expected.nice out.nice
