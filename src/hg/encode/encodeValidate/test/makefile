include ../../../../inc/common.mk

#PROG = ${SCRIPTS}/doEncodeValidate.pl
PROG = ../doEncodeValidate.pl

TEST_INSTALL_DIR = /cluster/data/encode/pipeline/tests

# DOWNLOAD_DIR is accessible via the web (http://hgwdev.cse.ucsc.edu/encode/tests)
DOWNLOAD_DIR = /usr/local/apache/htdocs/encode/tests

# Put the working dataType's into DIRS
DIRS = chipseq
FILES = load.ra trackDb.ra README.txt

all: test

bad:
	echo 'testing an un-readable file' > input/badDdf/unreadable.txt
	chmod 222 input/badDdf/unreadable.txt
	${PROG} -configDir=../config x input/badDdf

test:
	for dir in $(DIRS); do \
		${PROG} -allowReloads -configDir=../config x input/$$dir && \
		for file in $(FILES); do \
			diff --ignore-matching-lines=dateSubmitted input/$$dir/out/$$file expected/$$dir/$$file; \
		done \
        done

# update-expected let's you update the contents of the expected dir's (you still have
# to checkin to cvs (if you want to)).
update-expected:
	for dir in $(DIRS); do \
		${PROG} -allowReloads -configDir=../config x input/$$dir && \
		mkdir -p expected/$$dir && \
		for file in $(FILES); do \
			cp -p input/$$dir/out/$$file expected/$$dir; \
		done \
	done

output:
	${MKDIR} input/chipseq/out

clean: 
	rm -rf output
	rm -rf input/badDdf/unreadable.txt

install:
	for dir in $(DIRS); do \
		(cd input/$$dir; tar cvfz ${DOWNLOAD_DIR}/$$dir.good.tar.gz --exclude=CVS --exclude=out `cvsFiles`);\
	done
