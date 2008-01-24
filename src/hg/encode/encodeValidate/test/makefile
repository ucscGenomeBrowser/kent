include ../../../../inc/common.mk

#PROG = ${SCRIPTS}/doEncodeValidate.pl
PROG = ../doEncodeValidate.pl

all: test

test: dnase

dnase: output
	${PROG} x input/dnase -configDir=../config -outDir=output/dnase
	diff output/dnase/validateWig.out expected/dnase
	diff output/dnase/validateBed.out expected/dnase
	diff output/dnase/load.ra expected/dnase

dnaseBig: output dnaseBigLinks
	${PROG} x input/dnaseBig -configDir=../config -outDir=output/dnaseBig
	diff output/dnaseBig/validateWig.out expected/dnaseBig
	diff output/dnaseBig/validateBed.out expected/dnaseBig
	diff output/dnaseBig/load.ra expected/dnaseBig

output:
	${MKDIR} output/dnase output/dnaseBig

clean: 
	rm -rf output

DNASE_BIG_DATA = /cluster/data/hg17/bed/dukeDnase/2007-10-25/lab

dnaseBigClean:
	rm -f input/dnaseBig/*/*.{wig,bed}

dnaseBigLinks: dnaseBigClean
	ln -s ${DNASE_BIG_DATA}/chr1_dukeDnaseHsCd4Wiggle.out input/dnaseBig/cd4/chr1.wig 
	ln -s ${DNASE_BIG_DATA}/chr2_dukeDnaseHsCd4Wiggle.out input/dnaseBig/cd4/chr2.wig
	ln -s ${DNASE_BIG_DATA}/dukeDnaseHsCd4.bed input/dnaseBig/cd4/sites.bed
	ln -s ${DNASE_BIG_DATA}/chr1_dukeDnaseHsCd4Wiggle.out input/dnaseBig/hela/chr1.wig
	ln -s ${DNASE_BIG_DATA}/chr2_dukeDnaseHsCd4Wiggle.out input/dnaseBig/hela/chr2.wig
	ln -s ${DNASE_BIG_DATA}/dukeDnaseHsCd4.bed input/dnaseBig/hela/sites.bed

TEST_INSTALL_DIR = /cluster/data/encode/pipeline/tests

install:
	(cd input/dnase; tar cvfz ${TEST_INSTALL_DIR}/dnase.tar.gz --exclude=CVS --exclude=out * )

installBig: dnaseBigLinks
	(cd input/dnaseBig; tar cvfzh ${TEST_INSTALL_DIR}/dnaseBig.tar.gz --exclude=CVS --exclude=out * )

