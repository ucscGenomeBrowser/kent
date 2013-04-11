kentSrc = ../../..
A = txCdsPredict
include ${kentSrc}/inc/userApp.mk

test::
	txCdsPredict in.fa out.cds
	diff expected.cds out.cds
	txCdsPredict in.fa -nmd=in.bed outNmd.cds
	diff expectedNmd.cds outNmd.cds
	txCdsPredict in.fa -maf=in.maf outMaf.cds
	diff expectedMaf.cds outMaf.cds
