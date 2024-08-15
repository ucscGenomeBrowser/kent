kentSrc = ../../..
include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a
BIN_DIR = bin/${MACHTYPE}

all: 	${BIN_DIR}/genePredTester \
	${BIN_DIR}/pslReaderTester \
#	${BIN_DIR}/annoGratorTester \
	${BIN_DIR}/binTest \
	${BIN_DIR}/customTrackTester \
	${BIN_DIR}/hgvsTester \
	${BIN_DIR}/sqlCheck 

${BIN_DIR}/%: %.c ${MYLIBS}
	@${MKDIR} ${BIN_DIR}
	${CC} ${CC_PROG_OPTS} -o $@ $*.c ${MYLIBS} $L

#test: binTest spDbTest hdbTest genePredTest pslReaderTest annoGratorTest customTrackTest hgvsTest
test: binTest spDbTest hdbTest genePredTest pslReaderTest customTrackTest hgvsTest
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

sqlCheck: ${BIN_DIR}/sqlCheck mkdirs
	${MAKE} -f sqlCheck.mk test BIN_DIR=${BIN_DIR} OUT_DIR=output


clean:
	rm -rf *.o bin output *.tmp loader udcCache
