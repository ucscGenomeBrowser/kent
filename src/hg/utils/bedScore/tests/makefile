include ../../../../inc/common.mk

PROG = ../bedScore

test: encodeTest
	echo tested all

encodeTest:
	${PROG} input/encodeTest.bed output/encodeTest.bed
	@diff output/encodeTest.bed  expected/

