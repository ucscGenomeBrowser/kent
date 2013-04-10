kentSrc = ../../..
A = txCdsSvmInput
include ${kentSrc}/inc/userApp.mk

test::
	txCdsSvmInput in.bed in.tce in.ortho in.info out.lst out.vector
	diff expected.lst out.lst
	diff expected.vector out.vector
