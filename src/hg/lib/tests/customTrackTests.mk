# makefile with customTrackTester tests
include ../../../inc/common.mk
include testCommon.mk

TEST = customTrack
TESTER = ${BIN_DIR}/customTrackTester
IN = ${IN_DIR}/${TEST}
EXP = ${EXP_DIR}/${TEST}
OUT = ${OUT_DIR}/${TEST}

test: cgiUrl twoUrls simpleTest unnamed

unnamed: mkout ${IN}/unnamed.ct ${EXP}/unnamed.ct.bed
	${TESTER} parse ${IN}/unnamed.ct > ${OUT}/unnamed.ct.bed
	${TESTER} check ${OUT}/unnamed.ct.bed ${EXP}/unnamed.ct.bed

cgiUrl: mkout 
	${TESTER} parse ${IN}/cgiUrl.ct > ${OUT}/cgiUrl.bed

simpleTest: mkout ${IN}/simpleTest.ct ${EXP}/simpleTest.ct.bed
	${TESTER} parse ${IN}/simpleTest.ct > ${OUT}/simpleTest.ct.bed
	${TESTER} check ${OUT}/simpleTest.ct.bed ${EXP}/simpleTest.ct.bed

twoUrls: mkout ${IN}/twoUrls.ct ${EXP}/twoUrls.ct.bed
	${TESTER} parse ${IN}/twoUrls.ct > ${OUT}/twoUrls.ct.bed
	${TESTER} check ${OUT}/twoUrls.ct.bed ${EXP}/twoUrls.ct.bed

mkout:
	@${MKDIR} ${OUT}
