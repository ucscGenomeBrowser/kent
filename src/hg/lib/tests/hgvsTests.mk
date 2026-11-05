# makefile with hgHgvs tests
include ../../../inc/common.mk

BIN_DIR = bin/$(MACHTYPE)
TESTER = ${BIN_DIR}/hgvsTester
IN_DIR = input/hgvs
EXP_DIR = expected/hgvs
OUT_DIR = output/hgvs
DB=hg38

test: validTerms clinVarHgvs variants hugeVariants hugeTxVariants

validTerms: mkout
	${TESTER} ${DB} ${IN_DIR}/$@.txt > ${OUT_DIR}/$@.txt
	diff -u ${EXP_DIR}/$@.txt ${OUT_DIR}/$@.txt

clinVarHgvs: mkout
	${TESTER} ${DB} ${IN_DIR}/$@.txt > ${OUT_DIR}/$@.txt
	diff -u ${EXP_DIR}/$@.txt ${OUT_DIR}/$@.txt

variants: mkout
	${TESTER} -variants ${DB} ${IN_DIR}/$@.txt > ${OUT_DIR}/$@.txt
	diff -u ${EXP_DIR}/$@.txt ${OUT_DIR}/$@.txt

hugeVariants: mkout
	${TESTER} -variants -noTx ${DB} ${IN_DIR}/$@.txt > ${OUT_DIR}/$@.txt
	diff -u ${EXP_DIR}/$@.txt ${OUT_DIR}/$@.txt

hugeTxVariants: mkout
	${TESTER} -variants ${DB} ${IN_DIR}/$@.txt > ${OUT_DIR}/$@.txt
	diff -u ${EXP_DIR}/$@.txt ${OUT_DIR}/$@.txt

mkout:
	@${MKDIR} ${OUT_DIR}
