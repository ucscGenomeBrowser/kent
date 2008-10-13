include ../../../../inc/common.mk

#PROG = ${SCRIPTS}/doEncodeValidate.pl
PROG = ../doEncodeValidate.pl

TEST_INSTALL_DIR = /cluster/data/encode/pipeline/tests

# DOWNLOAD_DIR is accessible via the web (http://hgwdev.cse.ucsc.edu/encode/tests)
DOWNLOAD_DIR = /usr/local/apache/htdocs/encode/tests

# Put the working dataType's into DIRS
DIRS = chipseq
FILES = load.ra trackDb.ra README.txt

TMP_FILE_1 = 1.tmp
TMP_FILE_2 = 2.tmp

all: test

# "make bad" is useful for testing error reporting by doEncodeValidate.pl

bad:
	@echo 'testing an un-readable file' > input/badDdf/unreadable.txt
	@chmod 222 input/badDdf/unreadable.txt
	-${PROG} -configDir=../config x input/badDdf
	-${PROG} -configDir ../config x input/badDaf
	@rm input/badDdf/unreadable.txt

test:
	for dir in $(DIRS); do \
		${PROG} -allowReloads -configDir=../config x input/$$dir && \
		for file in $(FILES); do \
			egrep -v '(priority|dateSubmitted|data restricted)' input/$$dir/out/$$file > ${TMP_FILE_1} && \
			egrep -v '(priority|dateSubmitted|data restricted)' expected/$$dir/$$file > ${TMP_FILE_2} && \
			diff ${TMP_FILE_2} ${TMP_FILE_1} && \
			rm ${TMP_FILE_1} ${TMP_FILE_2}; \
		done \
        done
	echo "All tests passed"

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
