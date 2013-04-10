kentSrc = ../../..
A = bedWeedOverlapping
include ${kentSrc}/inc/userApp.mk

test::
	bedWeedOverlapping inWeed.bed input.bed output.bed
	diff expected.bed output.bed

clean::
	rm -f output.bed
