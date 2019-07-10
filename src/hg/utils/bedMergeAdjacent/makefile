kentSrc = ../../..
A = bedMergeAdjacent
include $(kentSrc)/inc/userApp.mk

clean::
	rm -f tests/output.bed 

test::
	rm -f tests/output.bed 
	${DESTBINDIR}/bedMergeAdjacent tests/input.bed tests/output.bed
	diff tests/expected.bed tests/output.bed
