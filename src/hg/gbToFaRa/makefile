kentSrc = ../..
A = gbToFaRa
include $(kentSrc)/inc/userApp.mk

test::
	${MKDIR} output
	gbToFaRa input/test.fil output/test.fa output/test.ra output/test.ta input/test.gb
	diff output/test.fa expected/test.fa
	diff output/test.ra expected/test.ra
	diff output/test.ta expected/test.ta
	rm -fr output

clean::
	rm -fr output
