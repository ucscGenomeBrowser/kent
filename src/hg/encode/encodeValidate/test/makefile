kentSrc = ../../../..
include ../../../../inc/common.mk

#PROG = ${SCRIPTS}/doEncodeValidate.pl
PROG = ../doEncodeValidate.pl

TEST_INSTALL_DIR = /cluster/data/encode/pipeline/tests

# DOWNLOAD_DIR is accessible via the web (http://hgwdev.soe.ucsc.edu/encode/tests)
DOWNLOAD_DIR = /usr/local/apache/htdocs/encode/tests

# Put the working dataType's into DIRS
DIRS = chipseq
BAD = badDaf badDdf
FILES = load.ra trackDb.ra  mdb.txt README.txt

TMP_FILE_1 = 1.tmp
TMP_FILE_2 = 2.tmp

all: test

# "make bad" is useful for testing error reporting by doEncodeValidate.pl

bad:
	@echo 'testing an un-readable file' > input/badDdf/unreadable.txt
	@chmod 222 input/badDdf/unreadable.txt
	for dir in $(BAD); do \
		${PROG} -configDir=../config x input/$$dir; \
	done
	@rm input/badDdf/unreadable.txt

test:
	for dir in $(DIRS); do \
		${PROG} -allowReloads -configDir=../config x input/$$dir && \
		for file in $(FILES); do \
                        echo $$dir/$$file && \
			rm -f ${TMP_FILE_1} ${TMP_FILE_2}; \
			egrep -v '(priority|dateSubmitted|dateUnrestricted|dataVersion|data RESTRICTED)' input/$$dir/out/$$file > ${TMP_FILE_1}; \
			egrep -v '(priority|dateSubmitted|dateUnrestricted|dataVersion|data RESTRICTED)' expected/$$dir/$$file > ${TMP_FILE_2}; \
			diff -b ${TMP_FILE_2} ${TMP_FILE_1}; \
                        if [ $$? -ne 0 ]; then exit 1; fi; \
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
	for dir in $(BAD); do \
		(cd input/$$dir; tar cvfz ${DOWNLOAD_DIR}/$$dir.bad.tar.gz --exclude=CVS --exclude=out `cvsFiles`);\
	done
