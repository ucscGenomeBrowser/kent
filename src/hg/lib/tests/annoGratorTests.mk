# makefile with annoGrator tests
include ../../../inc/common.mk

BIN_DIR = bin/$(MACHTYPE)
TESTER = ${BIN_DIR}/annoGratorTester
IN_DIR = input/annoGrator
EXP_DIR = expected/annoGrator
OUT_DIR = output/annoGrator
DB=hg19

test: pgSnpDbToTabOut pgSnpKgDbToTabOut

pgSnpDbToTabOut: mkout
	${TESTER} ${DB} $@ > ${OUT_DIR}/$@.txt
	diff -u ${EXP_DIR}/$@.txt ${OUT_DIR}/$@.txt

pgSnpKgDbToTabOut: mkout
	${TESTER} ${DB} $@ > ${OUT_DIR}/$@.txt
	diff -u ${EXP_DIR}/$@.txt ${OUT_DIR}/$@.txt

mkout:
	@${MKDIR} ${OUT_DIR}
