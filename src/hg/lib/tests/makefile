kentSrc = ../../..
include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a
BIN_DIR = bin/${MACHTYPE}

all: 	${BIN_DIR}/genePredTester \
	${BIN_DIR}/pslReaderTester \
	${BIN_DIR}/annoGratorTester \
	${BIN_DIR}/binTest \
	${BIN_DIR}/customTrackTester \
	${BIN_DIR}/hgvsTester \
	${BIN_DIR}/sqlCheck 

test: binTest spDbTest hdbTest genePredTest pslReaderTest annoGratorTest customTrackTest hgvsTest
	rm -r output
	echo tested all

mkdirs:
	@${MKDIR} output ${BIN_DIR}

loaders:
	rm -f loader
	ln -s ${CGI_BIN}/loader .

spDbTest: spDbTest.o ${MYLIBS} mkdirs
	${CC} ${COPT} -o ${BIN_DIR}/spDbTest spDbTest.o ${MYLIBS} $L
	${BIN_DIR}/spDbTest sp121210 Q9FFH7 > output/spDbTest
	${BIN_DIR}/spDbTest sp121210 P29312 >> output/spDbTest
	diff expected/spDbTest output/spDbTest

hdbTest: hdbTest.o ${MYLIBS} mkdirs
	${CC} ${COPT} -o ${BIN_DIR}/hdbTest hdbTest.o ${MYLIBS} $L
	${BIN_DIR}/hdbTest > output/hdbTest
	diff expected/hdbTest output/hdbTest

genePredTest: ${BIN_DIR}/genePredTester mkdirs
	${MAKE} -f genePredTests.mk test

pslReaderTest: ${BIN_DIR}/pslReaderTester mkdirs
	${MAKE} -f pslReaderTests.mk test

annoGratorTest: ${BIN_DIR}/annoGratorTester mkdirs
	${MAKE} -f annoGratorTests.mk test

customTrackTest: ${BIN_DIR}/customTrackTester loaders mkdirs
	${MAKE} -f customTrackTests.mk test BIN_DIR=${BIN_DIR} OUT_DIR=output
	rm -f loader

hgvsTest: ${BIN_DIR}/hgvsTester mkdirs
	${MAKE} -f hgvsTests.mk test

binTest: mkdirs ${BIN_DIR}/binTest
	@./binTest.sh

sqlCheck: ${BIN_DIR}/sqlCheck mkdirs
	${MAKE} -f sqlCheck.mk test BIN_DIR=${BIN_DIR} OUT_DIR=output


${BIN_DIR}/genePredTester: genePredTester.o ${MYLIBS}
	@${MKDIR} ${BIN_DIR}
	${CC} ${COPT} ${CFLAGS} -o $@ $< ${MYLIBS} ${L}

${BIN_DIR}/pslReaderTester: pslReaderTester.o ${MYLIBS}
	@${MKDIR} ${BIN_DIR}
	${CC} ${COPT} ${CFLAGS} -o $@ $< ${MYLIBS} ${L}

${BIN_DIR}/annoGratorTester: annoGratorTester.o ${MYLIBS}
	@${MKDIR} ${BIN_DIR}
	${CC} ${COPT} ${CFLAGS} -o $@ $< ${MYLIBS} ${L}

${BIN_DIR}/hgvsTester: hgvsTester.o ${MYLIBS}
	@${MKDIR} ${BIN_DIR}
	${CC} ${COPT} ${CFLAGS} -o $@ $< ${MYLIBS} ${L}

${BIN_DIR}/binTest: binTest.o ${MYLIBS}
	@${MKDIR} ${BIN_DIR}
	${CC} ${COPT} ${CFLAGS} -o $@ $< ${MYLIBS} ${L}

${BIN_DIR}/customTrackTester: customTrackTester.o ${MYLIBS}
	@${MKDIR} ${BIN_DIR}
	${CC} ${COPT} ${CFLAGS} -o $@ $< ${MYLIBS} ${L}

${BIN_DIR}/sqlCheck: sqlCheck.o ${MYLIBS}
	@${MKDIR} ${BIN_DIR}
	${CC} ${COPT} ${CFLAGS} -o $@ $< ${MYLIBS} ${L}

clean:
	rm -rf *.o bin output *.tmp loader udcCache
