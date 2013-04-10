kentSrc = ../../..
A = txCdsToGene
include ${kentSrc}/inc/userApp.mk

test::
	txCdsToGene in.bed in.fa in.tce output.gtf output.fa -bedOut=output.bed
	diff expected.fa output.fa
	diff expected.gtf output.gtf
	diff expected.bed output.bed
