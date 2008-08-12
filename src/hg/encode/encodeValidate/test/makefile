include ../../../../inc/common.mk

#PROG = ${SCRIPTS}/doEncodeValidate.pl
PROG = ../doEncodeValidate.pl

TEST_INSTALL_DIR = /cluster/data/encode/pipeline/tests

# DOWNLOAD_DIR is accessible via the web (http://hgwdev.cse.ucsc.edu/encode/tests)
DOWNLOAD_DIR = /usr/local/apache/htdocs/encode/tests

TMP_FILE_1 = trackDb.ra.tmp.1
TMP_FILE_2 = trackDb.ra.tmp.2

# Put the working dataType's into DIRS
DIRS = chipseq

all: test

test:
	for dir in $(DIRS); do \
		${PROG} -configDir=../config x input/$$dir && \
		diff input/$$dir/out/load.ra expected/$$dir/load.ra; \
		grep -v dateSubmitted input/$$dir/out/trackDb.ra > ${TMP_FILE_1} && \
		grep -v dateSubmitted expected/$$dir/trackDb.ra > ${TMP_FILE_2} && \
		diff ${TMP_FILE_1} ${TMP_FILE_2} && \
		rm ${TMP_FILE_1} ${TMP_FILE_2}; \
        done

output:
	${MKDIR} input/chipseq/out

clean: 
	rm -rf output

install:
	for dir in $(DIRS); do \
		(cd input/$$dir; tar cvfz ${DOWNLOAD_DIR}/$$dir.good.tar.gz --exclude=CVS --exclude=out `cvsFiles`);\
	done
