# makefile with pslReader tests
include ../../../inc/common.mk

BIN_DIR = bin/$(MACHTYPE)
PSLREADER_TESTER = ${BIN_DIR}/pslReaderTester
IN_DIR = input/pslReader
EXP_DIR = expected/pslReader
OUT_DIR = output/pslReader
DB=hg17
TEST_TBL=pslReaderTest_${USER}


test: fileTests tableTests

fileTests: fileNoHdr fileHdr

fileNoHdr: mkout
	${PSLREADER_TESTER} -output=${OUT_DIR}/$@.psl readFile ${IN_DIR}/mrna.psl
	diff -u ${IN_DIR}/mrna.psl ${OUT_DIR}/$@.psl

fileHdr: mkout ${OUT_DIR}/mrnaHdr.psl
	${PSLREADER_TESTER} -output=${OUT_DIR}/$@.psl readFile ${OUT_DIR}/mrnaHdr.psl
	diff -u ${IN_DIR}/mrna.psl ${OUT_DIR}/$@.psl

filePslx: mkout
	${PSLREADER_TESTER} -output=${OUT_DIR}/$@.psl readFile ${IN_DIR}/mrnax.psl
	diff -u ${EXP_DIR}/mrnax.psl ${OUT_DIR}/$@.psl

tableTests: tablePsl tablePslx
	hgsql -e "drop table if exists ${TEST_TBL}" ${DB}


tablePsl: mkout
	hgLoadPsl -verbose=0 -noHistory -table=${TEST_TBL} ${DB} ${IN_DIR}/mrna.psl
	${PSLREADER_TESTER} -output=${OUT_DIR}/$@.psl readTable ${DB} ${TEST_TBL}
	diff -u ${IN_DIR}/mrna.psl ${OUT_DIR}/$@.psl

tablePslx: mkout
	hgLoadPsl -verbose=0 -noHistory -table=${TEST_TBL} -xa ${DB} ${IN_DIR}/mrna.pslx
	${PSLREADER_TESTER} -output=${OUT_DIR}/$@.psl readTable ${DB} ${TEST_TBL}
	diff -u ${IN_DIR}/mrna.pslx ${OUT_DIR}/$@.psl



${OUT_DIR}/mrnaHdr.psl: ${IN_DIR}/mrna.psl mkout
	pslCat $< >$@

mkout:
	@${MKDIR} ${OUT_DIR}
