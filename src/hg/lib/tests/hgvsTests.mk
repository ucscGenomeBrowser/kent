# makefile with hgHgvs tests
include ../../../inc/common.mk

BIN_DIR = bin/$(MACHTYPE)
TESTER = ${BIN_DIR}/hgvsTester
IN_DIR = input/hgvs
EXP_DIR = expected/hgvs
OUT_DIR = output/hgvs
DB=hg38

test: validTerms clinVarHgvs

validTerms: mkout
	${TESTER} ${DB} ${IN_DIR}/$@.txt > ${OUT_DIR}/$@.txt
	diff -u ${EXP_DIR}/$@.txt ${OUT_DIR}/$@.txt

clinVarHgvs: mkout
	${TESTER} ${DB} ${IN_DIR}/$@.txt > ${OUT_DIR}/$@.txt
	diff -u ${EXP_DIR}/$@.txt ${OUT_DIR}/$@.txt

mkout:
	@${MKDIR} ${OUT_DIR}
