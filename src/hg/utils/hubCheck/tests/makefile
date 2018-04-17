kentSrc = ../../../..
include ../../../../inc/common.mk

hubCheck = ${DESTBINDIR}/hubCheck

test:: one two
	${MAKE} clean

one:	mkout
	${hubCheck} input/hub1.txt > output/output1.txt
	diff expected/output1.txt output/output1.txt

two:	mkout
	-${hubCheck} input/hub2.txt > output/output2.txt 2> /dev/null
	diff expected/output2.txt output/output2.txt

mkout::
	@mkdir -p output
clean::
	rm -rf output

