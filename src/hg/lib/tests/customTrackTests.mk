# makefile with customTrackTester tests
include ../../../inc/common.mk
include testCommon.mk

TEST = customTrack
TESTER = ${BIN_DIR}/customTrackTester
IN = ${IN_DIR}/${TEST}
EXP = ${EXP_DIR}/${TEST}
OUT = ${OUT_DIR}/${TEST}

test: simpleTest

simpleTest: mkout ${IN}/simpleTest.ct ${EXP}/simpleTest.ct.bed
	${TESTER} parse ${IN}/simpleTest.ct > ${OUT}/simpleTest.ct.bed
	${TESTER} check ${OUT}/simpleTest.ct.bed ${EXP}/simpleTest.ct.bed

mkout:
	@${MKDIR} ${OUT}
