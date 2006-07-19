include ../../../inc/common.mk
HG_WARN = ${HG_WARN_ERR}

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a
BIN_DIR = bin/${MACHTYPE}

all: 	${BIN_DIR}/genePredTester \
	${BIN_DIR}/pslReaderTester \
	${BIN_DIR}/binTest

test: binTest spDbTest hdbTest genePredTest pslReaderTest
	rm -r output
	echo tested all

mkdirs:
	@${MKDIR} output ${BIN_DIR}

spDbTest: spDbTest.o ${MYLIBS} mkdirs
	${CC} ${COPT} -o ${BIN_DIR}/spDbTest spDbTest.o ${MYLIBS} $L
	${BIN_DIR}/spDbTest uniProt Q9FFH7 > output/spDbTest
	${BIN_DIR}/spDbTest uniProt P29312 >> output/spDbTest
	diff expected/spDbTest output/spDbTest

hdbTest: hdbTest.o ${MYLIBS} mkdirs
	${CC} ${COPT} -o ${BIN_DIR}/hdbTest hdbTest.o ${MYLIBS} $L
	${BIN_DIR}/hdbTest > output/hdbTest
	diff expected/hdbTest output/hdbTest

genePredTest: ${BIN_DIR}/genePredTester mkdirs
	${MAKE} -f genePredTests.mk test

pslReaderTest: ${BIN_DIR}/pslReaderTester mkdirs
	${MAKE} -f pslReaderTests.mk test

binTest: mkdirs ${BIN_DIR}/binTest
	@./binTest.sh

${BIN_DIR}/genePredTester: genePredTester.o ${MYLIBS}
	@${MKDIR} ${BIN_DIR}
	${CC} ${COPT} ${CFLAGS} -o $@ $< ${MYLIBS} ${L}

${BIN_DIR}/pslReaderTester: pslReaderTester.o ${MYLIBS}
	@${MKDIR} ${BIN_DIR}
	${CC} ${COPT} ${CFLAGS} -o $@ $< ${MYLIBS} ${L}

${BIN_DIR}/binTest: binTest.o ${MYLIBS}
	@${MKDIR} ${BIN_DIR}
	${CC} ${COPT} ${CFLAGS} -o $@ $< ${MYLIBS} ${L}

clean:
	rm -rf *.o bin output *.tmp
