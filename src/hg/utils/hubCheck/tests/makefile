kentSrc = ../../../..
include ../../../../inc/common.mk

hubCheck = ${DESTBINDIR}/hubCheck

test:: one two errors
	${MAKE} clean

one:	mkout
	-${hubCheck} input/hub1.txt > output/output1.txt
	diff expected/output1.txt output/output1.txt

two:	mkout
	-${hubCheck} input/hub2.txt > output/output2.txt 2> /dev/null
	diff expected/output2.txt output/output2.txt

supertrackChild: mkout
	-${hubCheck} input/supertrackChild.txt > output/supertrackChild.txt 2> /dev/null
	diff expected/supertrackChild.txt output/supertrackChild.txt

errors:	mkout
	-${hubCheck} input/manyErrorsHub.txt > output/manyErrors.output.txt 2>/dev/null
	diff expected/manyErrors.output.txt output/manyErrors.output.txt

mkout::
	@mkdir -p output
clean::
	rm -rf output

