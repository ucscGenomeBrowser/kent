A = intervalToBed

$A:
	paraFlow ${A}.pf

test: $A
	./$A test.int test.bed
	diff test.bed expected.bed

clean:
	rm $A ${A}.{o,pfh,c} out.*
