include ../../../../inc/common.mk

#PROG = ${SCRIPTS}/doEncodeValidate.pl
PROG = ../doEncodeValidate.pl

TEST_INSTALL_DIR = /cluster/data/encode/pipeline/tests

all: test

test: duke

duke: output
	${PROG} x input/duke -configDir=../config -outDir=output/duke
	diff output/duke/load.ra expected/duke/load.ra
# Can't diff trackDb.ra, because dateSubmitted will be different
#	diff output/duke/trackDb.ra expected/duke

# Obsolete tests

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

dnaseHuge: output dnaseHugeLinks
	${PROG} x input/dnaseHuge -configDir=../config -outDir=output/dnaseHuge
	diff output/dnaseHuge/validateWig.out expected/dnaseHuge
	diff output/dnaseHuge/validateBed.out expected/dnaseHuge
	diff output/dnaseHuge/load.ra expected/dnaseHuge

genes: output
	${PROG} x input/genes -configDir=../config -outDir=output/genes
	diff output/genes/validateGFF.out expected/genes
	diff output/genes/load.ra expected/genes

bed3: output
	${PROG} x input/bed3 -configDir=../config -outDir=output/bed3
	diff output/bed3/validateBed.out expected/bed3
	diff output/bed3/load.ra expected/bed3

output:
	${MKDIR} output/dnase output/dnaseBig output/genes output/bed3 output/dnaseHuge

clean: 
	rm -rf output

DNASE_BIG_DATA = /cluster/data/hg17/bed/dukeDnase/2007-10-25/lab

dnaseBigClean:
	rm -f input/dnaseBig/*/*.{wig,bed}

dnaseHugeClean:
	rm -f input/dnaseHuge/*/*.{wig,bed}

dnaseBigLinks: dnaseBigClean
	mkdir -p input/dnaseBig/{cd4,hela}
	ln -s ${DNASE_BIG_DATA}/chr1_dukeDnaseHsCd4Wiggle.out \
                    input/dnaseBig/cd4/chr1.wig 
	ln -s ${DNASE_BIG_DATA}/chr2_dukeDnaseHsCd4Wiggle.out \
                    input/dnaseBig/cd4/chr2.wig
	ln -s ${DNASE_BIG_DATA}/dukeDnaseHsCd4.bed \
		    input/dnaseBig/cd4/sites.bed
	ln -s ${DNASE_BIG_DATA}/chr1_dukeDnaseHsCd4Wiggle.out \
                    input/dnaseBig/hela/chr1.wig
	ln -s ${DNASE_BIG_DATA}/chr2_dukeDnaseHsCd4Wiggle.out \
                    input/dnaseBig/hela/chr2.wig
	ln -s ${DNASE_BIG_DATA}/dukeDnaseHsCd4.bed \
                    input/dnaseBig/hela/sites.bed

dnaseHugeLinks: dnaseHugeClean
	mkdir -p input/dnaseHuge/cd4
	ln -s ${DNASE_BIG_DATA}/dukeDnaseHsCd4.bed input/dnaseHuge/cd4/sites.bed
	@for C in `seq 1 22`; do \
	    ln -s ${DNASE_BIG_DATA}/chr$${C}_dukeDnaseHsCd4Wiggle.out \
	        input/dnaseHuge/cd4/chr$${C}.wig; \
	done


install:
	tar cvfz ${TEST_INSTALL_DIR}/bed3.good.tar.gz \
                    -C input/bed3 bed3.PIF chr1.bed chr2.bed chr3.bed -C DDFs bed3.3files.DDF
	tar cvfz ${TEST_INSTALL_DIR}/bed3.bad.tar.gz \
                    -C input/bed3 bed3.PIF bed3.2files.DDF chr2.bed \
                    -C bad chr1.bed
	tar cvfz ${TEST_INSTALL_DIR}/bed3.fix.tar.gz -C input/bed3 chr1.bed
	tar cvfz ${TEST_INSTALL_DIR}/bed3.update.tar.gz \
                    -C input/bed3 chr3.bed -C DDFs bed3.3files.DDF
	(cd input/dnase; tar cvfz ${TEST_INSTALL_DIR}/dnase.tar.gz --exclude=CVS --exclude=out *)
	(cd input/genes; tar cvfz ${TEST_INSTALL_DIR}/genes.tar.gz --exclude=CVS --exclude=out *)

installBig: dnaseBigLinks
	(cd input/dnaseBig; nice tar cvfzh ${TEST_INSTALL_DIR}/dnaseBig.tar.gz \
                    -exclude=CVS --exclude=out *

installHuge: dnaseHugeLinks
	(cd input/dnaseHuge; nice tar cvfzh ${TEST_INSTALL_DIR}/dnaseHuge.tar.gz \
                    --exclude=CVS --exclude=out * )

