kentSrc = ../../..
A = txgAnalyze
include ${kentSrc}/inc/userApp.mk

test::
	txgAnalyze input.txg /cluster/data/hg18/hg18.2bit output.bed
	diff expected.bed output.bed

clean::
	rm -f output.bed
