kentSrc = ../../..
A = txCdsRepick
include ${kentSrc}/inc/userApp.mk

test::
	txCdsRepick in.bed in.txg in.cluster in.info in.tce out.tce out.pp
	diff expected.tce out.tce
	diff expected.pp out.pp
