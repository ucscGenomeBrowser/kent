kentSrc = ../../..
A = txCdsEvFromRna
include ${kentSrc}/inc/userApp.mk

test::
	txCdsEvFromRna inRna.fa in.cds in.psl inTx.fa output.ev -refStatus=inRefStatus.tab -unmapped=output.unmapped
	diff expected.ev output.ev
	diff expected.unmapped output.unmapped
