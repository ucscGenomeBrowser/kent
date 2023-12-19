kentSrc = ../../../..
include ../../../../inc/common.mk

hubCheck = ${DESTBINDIR}/hubCheck

test:: one two errors supertrackChild genomesErrors badType badSubgroups autoScale noLongLabel
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

genomesErrors: mkout
	-${hubCheck} input/genomesConfigErrorsHub.txt > output/genomesConfigErrors.output.txt 2>/dev/null
	diff expected/genomesConfigErrors.output.txt output/genomesConfigErrors.output.txt

badType: mkout
	-${hubCheck} input/badType.txt > output/badType.output.txt 2>/dev/null
	diff expected/badType.output.txt output/badType.output.txt

badSubgroups: mkout
	-${hubCheck} input/badSubGroups.txt > output/badSubGroups.output.txt 2>/dev/null
	diff expected/badSubGroups.output.txt output/badSubGroups.output.txt

autoScale: mkout
	-${hubCheck} input/autoScaleHub.txt > output/autoScaleHub.output.txt 2>/dev/null
	diff expected/autoScaleHub.txt output/autoScaleHub.output.txt

noLongLabel:	mkout
	-${hubCheck} input/noLongLabel.txt > output/noLongLabel.txt
	diff expected/noLongLabel.txt output/noLongLabel.txt

mkout::
	@mkdir -p output
clean::
	rm -rf output

