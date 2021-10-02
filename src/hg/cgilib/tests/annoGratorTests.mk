# makefile with annoGrator tests
include ../../../inc/common.mk

BIN_DIR = bin/$(MACHTYPE)
TESTER = ${BIN_DIR}/annoGratorTester
IN_DIR = input/annoGrator
EXP_DIR = expected/annoGrator
OUT_DIR = output/annoGrator
DB=hg19

test: pgSnpDbToTabOut pgSnpKgDbToTabOutShort pgSnpKgDbToGpFx snpConsDbToTabOutShort vcfEx1 vcfEx2 \
      bigBedToTabOut snpBigWigToTabOut vepOut vepOutIndelTrim gpFx insertions

pgSnpDbToTabOut: mkout
	${TESTER} ${DB} $@ > ${OUT_DIR}/$@.txt
	diff -u ${EXP_DIR}/$@.txt ${OUT_DIR}/$@.txt

pgSnpKgDbToTabOutShort: mkout
	${TESTER} ${DB} $@ > ${OUT_DIR}/$@.txt
	diff -u ${EXP_DIR}/$@.txt ${OUT_DIR}/$@.txt

pgSnpKgDbToGpFx: mkout
	${TESTER} ${DB} $@ > ${OUT_DIR}/$@.txt
	diff -u ${EXP_DIR}/$@.txt ${OUT_DIR}/$@.txt

snpConsDbToTabOutShort: mkout
	${TESTER} ${DB} $@ > ${OUT_DIR}/$@.txt
	diff -u ${EXP_DIR}/$@.txt ${OUT_DIR}/$@.txt

vcfEx1: mkout
	${TESTER} ${DB} $@ > ${OUT_DIR}/$@.txt
	diff -u ${EXP_DIR}/$@.txt ${OUT_DIR}/$@.txt

vcfEx2: mkout
	${TESTER} ${DB} $@ > ${OUT_DIR}/$@.txt
	diff -u ${EXP_DIR}/$@.txt ${OUT_DIR}/$@.txt

bigBedToTabOut: mkout
	${TESTER} ${DB} $@ > ${OUT_DIR}/$@.txt
	diff -u ${EXP_DIR}/$@.txt ${OUT_DIR}/$@.txt

snpBigWigToTabOut: mkout
	${TESTER} ${DB} $@ > ${OUT_DIR}/$@.txt
	diff -u ${EXP_DIR}/$@.txt ${OUT_DIR}/$@.txt

vepOut: mkout
	${TESTER} ${DB} $@ | grep -v 'Output produced at' > ${OUT_DIR}/$@.txt
	diff -u ${EXP_DIR}/$@.txt ${OUT_DIR}/$@.txt

vepOutIndelTrim: mkout
	${TESTER} ${DB} $@ | grep -v 'Output produced at' > ${OUT_DIR}/$@.txt
	diff -u ${EXP_DIR}/$@.txt ${OUT_DIR}/$@.txt

gpFx: mkout
	${TESTER} ${DB} $@ | grep -v 'Output produced at' > ${OUT_DIR}/$@.txt
	diff -u ${EXP_DIR}/$@.txt ${OUT_DIR}/$@.txt

insertions: mkout
	hgsql test -e 'drop table if exists chromInfo; \
	               create table chromInfo select * from hg19.chromInfo;'
	hgLoadBed -verbose=0 -allowStartEqualEnd \
                  test insertionsPrimary input/annoGrator/insertionsPrimary.bed
	hgLoadBed -verbose=0 -allowStartEqualEnd \
                  test insertionsSecondary input/annoGrator/insertionsSecondary.bed
	${TESTER} ${DB} $@ > ${OUT_DIR}/$@.txt
	diff -u ${EXP_DIR}/$@.txt ${OUT_DIR}/$@.txt
	hgsql test -e 'drop table chromInfo; \
                       drop table insertionsPrimary; \
                       drop table insertionsSecondary;'
	rm -f bed.tab

mkout:
	@${MKDIR} ${OUT_DIR}
