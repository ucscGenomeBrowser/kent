include ../../../inc/common.mk
HG_WARN_ERR = -DJK_WARN -Wall -Werror

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a
BIN_DIR = bin/$(MACHTYPE)

all: ${BIN_DIR}/genePredTester

test: spDbTest hdbTest genePredTest
	rm -r output
	echo tested all

mkdirs:
	${MKDIR} output ${BIN_DIR}

spDbTest: spDbTest.o $(MYLIBS) mkdirs
	gcc -ggdb -o ${BIN_DIR}/spDbTest spDbTest.o $(MYLIBS) $L
	${BIN_DIR}/spDbTest swissProt Q9FFH7 > output/spDbTest
	${BIN_DIR}/spDbTest swissProt P29312 >> output/spDbTest
	diff expected/spDbTest output/spDbTest

hdbTest: hdbTest.o $(MYLIBS) mkdirs
	gcc -ggdb -o ${BIN_DIR}/hdbTest hdbTest.o $(MYLIBS) $L
	${BIN_DIR}/hdbTest > output/hdbTest
	diff expected/hdbTest output/hdbTest

genePredTest: ${BIN_DIR}/genePredTester mkdirs
	${MAKE} -f genePredTests.mk test

${BIN_DIR}/genePredTester: genePredTester.o ${MYLIBS}
	@${MKDIR} ${BIN_DIR}
	${CC} ${COPT} ${CFLAGS} -o $@ $< ${MYLIBS} ${L}

clean:
	rm -rf *.o bin output
