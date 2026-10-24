# Unit tests that link the hgTracks objects, everything but mainMain.o, in place of the
# Playwright tests in ../tests.  hgTracks' own "make test" builds the objects and passes their
# list in COMMONO.
kentSrc = ../../..
include ../../../inc/common.mk

HG_INC += -I.. -I../../optimalLeaf
L += ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS = ${MYLIBDIR}/jkhgapcgi.a ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a ${MYLIBDIR}/optimalLeaf.a
HGTRACKSO = $(addprefix ../,${COMMONO}) hgTracksStubs.c
BIN_DIR = bin/${MACHTYPE}

test: exonFramePhasesTest itemCoverageTest bigWigLoadPreDrawTest refGeneColorQueriesTest
	rm -r output
	@echo tested all

mkdirs:
	@if [ -z "${COMMONO}" ]; then echo "run make test from hgTracks, which sets COMMONO"; exit 1; fi
	@${MKDIR} output ${BIN_DIR}

exonFramePhasesTest: mkdirs
	${CC} ${CC_PROG_OPTS} ${HG_INC} -o ${BIN_DIR}/exonFramePhasesTester exonFramePhasesTester.c ${HGTRACKSO} ${MYLIBS} ${L}
	${BIN_DIR}/exonFramePhasesTester > output/exonFramePhasesTest
	diff expected/exonFramePhasesTest output/exonFramePhasesTest

itemCoverageTest: mkdirs
	${CC} ${CC_PROG_OPTS} ${HG_INC} -o ${BIN_DIR}/itemCoverageTester itemCoverageTester.c ${HGTRACKSO} ${MYLIBS} ${L}
	${BIN_DIR}/itemCoverageTester > output/itemCoverageTest
	diff expected/itemCoverageTest output/itemCoverageTest

bigWigLoadPreDrawTest: mkdirs
	${CC} ${CC_PROG_OPTS} ${HG_INC} -o ${BIN_DIR}/bigWigLoadPreDrawTester bigWigLoadPreDrawTester.c ${HGTRACKSO} ${MYLIBS} ${L}
	${BIN_DIR}/bigWigLoadPreDrawTester input/quickLiftGap > output/bigWigLoadPreDrawTest
	diff expected/bigWigLoadPreDrawTest output/bigWigLoadPreDrawTest

# Reads hg38 refGene and hgFixed.refSeqStatus.
refGeneColorQueriesTest: mkdirs
	${CC} ${CC_PROG_OPTS} ${HG_INC} -o ${BIN_DIR}/refGeneColorQueriesTester refGeneColorQueriesTester.c ${HGTRACKSO} ${MYLIBS} ${L}
	${BIN_DIR}/refGeneColorQueriesTester > output/refGeneColorQueriesTest
	diff expected/refGeneColorQueriesTest output/refGeneColorQueriesTest

clean:
	rm -rf bin output
