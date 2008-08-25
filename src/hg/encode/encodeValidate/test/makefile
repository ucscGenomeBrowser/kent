include ../../../../inc/common.mk

#PROG = ${SCRIPTS}/doEncodeValidate.pl
PROG = ../doEncodeValidate.pl

TEST_INSTALL_DIR = /cluster/data/encode/pipeline/tests

# DOWNLOAD_DIR is accessible via the web (http://hgwdev.cse.ucsc.edu/encode/tests)
DOWNLOAD_DIR = /usr/local/apache/htdocs/encode/tests

# Put the working dataType's into DIRS
DIRS = chipseq

all: test

test:
	for dir in $(DIRS); do \
		${PROG} -allowReloads -configDir=../config x input/$$dir && \
		diff input/$$dir/out/load.ra expected/$$dir/load.ra && \
		diff --ignore-matching-lines=dateSubmitted input/$$dir/out/trackDb.ra expected/$$dir/trackDb.ra; \
        done

output:
	${MKDIR} input/chipseq/out

clean: 
	rm -rf output

install:
	for dir in $(DIRS); do \
		(cd input/$$dir; tar cvfz ${DOWNLOAD_DIR}/$$dir.good.tar.gz --exclude=CVS --exclude=out `cvsFiles`);\
	done
