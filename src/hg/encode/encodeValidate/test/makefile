include ../../../../inc/common.mk

#PROG = ${SCRIPTS}/doEncodeValidate.pl
PROG = ../doEncodeValidate.pl

TEST_INSTALL_DIR = /cluster/data/encode/pipeline/tests

DOWNLOAD_DIR = /usr/local/apache/htdocs/encode/tests

all: test

test: chipseq

chipseq: output
	${PROG} x input/chipseq -configDir=../config -outDir=output/chipseq
	diff output/chipseq/load.ra expected/chipseq/load.ra

# duke is an example of DNase-Seq
duke: output
	${PROG} x input/duke -configDir=../config -outDir=output/duke
	diff output/duke/load.ra expected/duke/load.ra
# Can't diff trackDb.ra, because dateSubmitted will be different
#	diff output/duke/trackDb.ra expected/duke

output:
	${MKDIR} output/duke

clean: 
	rm -rf output

install:
	(cd input/duke; tar cvfz ${DOWNLOAD_DIR}/dnase.good.tar.gz --exclude=CVS --exclude=out *)
