kentSrc = ../../..
A = bedMergeAdjacent
include $(kentSrc)/inc/userApp.mk

clean::
	rm -f tests/test-output.bed 

test::
	rm -f tests/test-output.bed 
	${DESTBINDIR}/bedMergeAdjacent tests/test-input.bed tests/test-output.bed
	diff tests/test-expected.bed tests/test-output.bed
