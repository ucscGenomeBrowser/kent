kentSrc = ../../..
A = txPslToBed
include ${kentSrc}/inc/userApp.mk

test::
	txPslToBed -verbose=0 -cds=input.cds input.psl /cluster/data/hg18/hg18.2bit output.bed 
#	txPslToBed -verbose=0 input.psl /cluster/data/hg18/hg18.2bit output.bed 
	diff expected.bed output.bed

clean::
	rm -f output.bed
