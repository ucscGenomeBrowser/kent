kentSrc = ../../..
include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a
BIN_DIR = bin/${MACHTYPE}

all: 	${BIN_DIR}/genePredTester \
	${BIN_DIR}/trashDirTester \
	${BIN_DIR}/mallocTopPadTester \
	${BIN_DIR}/pslReaderTester \
	${BIN_DIR}/quickLiftTester \
	${BIN_DIR}/sessionDataTester \
	${BIN_DIR}/trashDirTester \
#	${BIN_DIR}/annoGratorTester \
	${BIN_DIR}/binTest \
	${BIN_DIR}/customTrackTester \
	${BIN_DIR}/hgvsTester \
	${BIN_DIR}/sqlCheck 

${BIN_DIR}/%: %.c ${MYLIBS}
	@${MKDIR} ${BIN_DIR}
	${CC} ${CC_PROG_OPTS} -o $@ $*.c ${MYLIBS} $L

#test: binTest spDbTest hdbTest genePredTest pslReaderTest annoGratorTest customTrackTest hgvsTest
test: binTest quickLiftTest sessionDataTest trashDirTest mallocTopPadTest \
	hVarSubstHtmlTest dataVersionPathTest genarkLiftOverTest \
	snapshotTypeTest sessionDirTest geoMirrorSelfTest spDbTest hdbTest genePredTest pslReaderTest customTrackTest hgvsTest
	rm -r output
	echo tested all

mkdirs:
	@${MKDIR} output ${BIN_DIR}

loaders:
	ln -sf ${CGI_BIN}/loader .

spDbTest: ${BIN_DIR}/spDbTest mkdirs
	${BIN_DIR}/spDbTest sp121210 Q9FFH7 > output/spDbTest
	${BIN_DIR}/spDbTest sp121210 P29312 >> output/spDbTest
	diff expected/spDbTest output/spDbTest

hdbTest: ${BIN_DIR}/hdbTest mkdirs
	${BIN_DIR}/hdbTest > output/hdbTest
	diff expected/hdbTest output/hdbTest

genePredTest: ${BIN_DIR}/genePredTester mkdirs
	${MAKE} -f genePredTests.mk test

pslReaderTest: ${BIN_DIR}/pslReaderTester mkdirs
	${MAKE} -f pslReaderTests.mk test

#annoGratorTest: ${BIN_DIR}/annoGratorTester mkdirs
#	${MAKE} -f annoGratorTests.mk test

customTrackTest: ${BIN_DIR}/customTrackTester loaders mkdirs
	${MAKE} -f customTrackTests.mk test BIN_DIR=${BIN_DIR} OUT_DIR=output

hgvsTest: ${BIN_DIR}/hgvsTester mkdirs
	${MAKE} -f hgvsTests.mk test

binTest: mkdirs ${BIN_DIR}/binTest
	@./binTest.sh

quickLiftTest: ${BIN_DIR}/quickLiftTester mkdirs
	${BIN_DIR}/quickLiftTester > output/quickLiftTest
	diff expected/quickLiftTest output/quickLiftTest

sessionDataTest: ${BIN_DIR}/sessionDataTester mkdirs
	${BIN_DIR}/sessionDataTester output/sessionData > output/sessionDataTest
	diff expected/sessionDataTest output/sessionDataTest

# Three runs, because hgConfig caches the config it read, so one process can only answer for
# one spelling of sessionDataDir.  The fixture is made here rather than in C: a directory, a
# symlink to it, and three confs naming the same place three ways.  Each conf includes the
# developer's own so the database settings come along, and is mode 600 because hgConfig
# refuses a group-readable file whose name begins with a dot -- these do not, but the habit is
# worth keeping.
trashDirTest: ${BIN_DIR}/trashDirTester mkdirs
	rm -rf output/sd
	${MKDIR} output/sd/real/hgt
	ln -s real output/sd/link
	echo "include ${HOME}/.hg.conf" > output/sd/link.conf
	echo "sessionDataDir=`pwd`/output/sd/link" >> output/sd/link.conf
	echo "include ${HOME}/.hg.conf" > output/sd/real.conf
	echo "sessionDataDir=`pwd`/output/sd/real" >> output/sd/real.conf
	echo "include ${HOME}/.hg.conf" > output/sd/rel.conf
	echo "sessionDataDir=output/sd/link" >> output/sd/rel.conf
	HGDB_CONF=output/sd/link.conf ${BIN_DIR}/trashDirTester \
	    "the symlink, absolute" `pwd`/output/sd/link `pwd`/output/sd/real \
	    > output/trashDirTest
	HGDB_CONF=output/sd/real.conf ${BIN_DIR}/trashDirTester \
	    "the resolved directory, absolute" `pwd`/output/sd/link `pwd`/output/sd/real \
	    >> output/trashDirTest
	HGDB_CONF=output/sd/rel.conf ${BIN_DIR}/trashDirTester \
	    "the symlink, relative" `pwd`/output/sd/link `pwd`/output/sd/real \
	    >> output/trashDirTest
	diff expected/trashDirTest output/trashDirTest

# Twice: once with the setting and once without, because the answer is the DIFFERENCE between
# them.  A single run could pass on a library whose own default happened to be large.
mallocTopPadTest: ${BIN_DIR}/mallocTopPadTester mkdirs
	echo "include ${HOME}/.hg.conf" > output/topPadOff.conf
	echo "include ${HOME}/.hg.conf" > output/topPadOn.conf
	echo "mallocTopPad=33554432" >> output/topPadOn.conf
	HGDB_CONF=output/topPadOff.conf ${BIN_DIR}/mallocTopPadTester > output/mallocTopPadTest
	HGDB_CONF=output/topPadOn.conf ${BIN_DIR}/mallocTopPadTester >> output/mallocTopPadTest
	diff expected/mallocTopPadTest output/mallocTopPadTest

hVarSubstHtmlTest: mkdirs
	${CC} ${CC_PROG_OPTS} -o ${BIN_DIR}/hVarSubstHtmlTester hVarSubstHtmlTester.c ${MYLIBS} $L
	${BIN_DIR}/hVarSubstHtmlTester > output/hVarSubstHtmlTest
	diff expected/hVarSubstHtmlTest output/hVarSubstHtmlTest

dataVersionPathTest: mkdirs
	${CC} ${CC_PROG_OPTS} -o ${BIN_DIR}/dataVersionPathTester dataVersionPathTester.c ${MYLIBS} $L
	${BIN_DIR}/dataVersionPathTester > output/dataVersionPathTest
	diff expected/dataVersionPathTest output/dataVersionPathTest

genarkLiftOverTest: mkdirs
	${CC} ${CC_PROG_OPTS} -o ${BIN_DIR}/genarkLiftOverTester genarkLiftOverTester.c ${MYLIBS} $L
	${BIN_DIR}/genarkLiftOverTester > output/genarkLiftOverTest
	diff expected/genarkLiftOverTest output/genarkLiftOverTest

snapshotTypeTest: mkdirs
	${CC} ${CC_PROG_OPTS} -o ${BIN_DIR}/snapshotTypeTester snapshotTypeTester.c ${MYLIBS} $L
	${BIN_DIR}/snapshotTypeTester > output/snapshotTypeTest
	diff expected/snapshotTypeTest output/snapshotTypeTest

sessionDirTest: mkdirs
	${CC} ${CC_PROG_OPTS} -o ${BIN_DIR}/sessionDirTester sessionDirTester.c ${MYLIBS} $L
	${BIN_DIR}/sessionDirTester > output/sessionDirTest
	diff expected/sessionDirTest output/sessionDirTest

# browser.node is set in the conf rather than assumed, since a developer's own hg.conf need
# not have one.
geoMirrorSelfTest: mkdirs
	${CC} ${CC_PROG_OPTS} -o ${BIN_DIR}/geoMirrorSelfTester geoMirrorSelfTester.c ${MYLIBS} $L
	echo "include ${HOME}/.hg.conf" > output/geoNode.conf
	echo "browser.node=1" >> output/geoNode.conf
	HGDB_CONF=output/geoNode.conf ${BIN_DIR}/geoMirrorSelfTester > output/geoMirrorSelfTest
	diff expected/geoMirrorSelfTest output/geoMirrorSelfTest

# NOT in the test target above, and waiting on one grant.  This is the one test here that
# WRITES to a central database, so it is pointed at hgcentralregress,
# which exists for tests and which nothing else reads.  makeHgCentralRegress.sh creates it and
# is safe to run every time.  login.cookieSalt is set because the sync signature is made with
# it; the value is a test value and means nothing outside this run.
hubSpaceKeysTest: mkdirs
	${CC} ${CC_PROG_OPTS} -o ${BIN_DIR}/hubSpaceKeysTester hubSpaceKeysTester.c ${MYLIBS} $L
	./makeHgCentralRegress.sh > /dev/null
	echo "include ${HOME}/.hg.conf" > output/regress.conf
	echo "central.db=hgcentralregress" >> output/regress.conf
	echo "login.cookieSalt=testSaltForRegressRuns" >> output/regress.conf
	HGDB_CONF=output/regress.conf ${BIN_DIR}/hubSpaceKeysTester > output/hubSpaceKeysTest
	diff expected/hubSpaceKeysTest output/hubSpaceKeysTest
# It runs as soon as central.user has UPDATE and DELETE on hgcentralregress; see
# makeHgCentralRegress.sh for the grant.  Add it to the test target on that day.

sqlCheck: ${BIN_DIR}/sqlCheck mkdirs
	${MAKE} -f sqlCheck.mk test BIN_DIR=${BIN_DIR} OUT_DIR=output


clean:
	rm -rf *.o bin output *.tmp loader udcCache
