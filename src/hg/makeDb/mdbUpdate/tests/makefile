include ../../../../inc/common.mk

all:

test: chipSeqTests

chipSeqTests: chipSeqTest1

# create test table
# load and display using GM12878 an BCL11A
chipSeqTest1: mkdirs
	mdbUpdate hg18 -recreate -table=testMetaDb input/chipSeqTest1.ra
	mdbPrint hg18 -table=testMetaDb -all > output/chipSeqTest1.ra
	diff output/chipSeqTest1.ra expected/

clean::
	rm -rf output

mkdirs:
	@${MKDIR} output

