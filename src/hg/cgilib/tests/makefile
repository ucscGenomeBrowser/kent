kentSrc = ../../..
include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgapcgi.a ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a
BIN_DIR = bin/${MACHTYPE}

all: 	${BIN_DIR}/annoGratorTester 

# annoGratorTest is left out: it needs assemblies and tables this directory cannot assume.
# Until bedItemRgbTest arrived (refs #38391) that made `make test` here a no-op that printed
# "tested all", which hg/makefile has been running all along through TEST_EXTRA.
#test: annoGratorTest
test: bedItemRgbTest
	rm -rf output
	echo tested all

mkdirs:
	@${MKDIR} output ${BIN_DIR}

# Two runs, because the last step of the rule reads hg.conf's alwaysItemRgb and a mirror may
# turn it off.
bedItemRgbTest: mkdirs
	${CC} ${CC_PROG_OPTS} -o ${BIN_DIR}/bedItemRgbTester bedItemRgbTester.c ${MYLIBS} $L
	echo "include ${HOME}/.hg.conf" > output/rgbOn.conf
	echo "include ${HOME}/.hg.conf" > output/rgbOff.conf
	echo "alwaysItemRgb=off" >> output/rgbOff.conf
	HGDB_CONF=output/rgbOn.conf ${BIN_DIR}/bedItemRgbTester > output/bedItemRgbTest
	HGDB_CONF=output/rgbOff.conf ${BIN_DIR}/bedItemRgbTester >> output/bedItemRgbTest
	diff expected/bedItemRgbTest output/bedItemRgbTest

annoGratorTest: ${BIN_DIR}/annoGratorTester mkdirs
	${MAKE} -f annoGratorTests.mk test

${BIN_DIR}/annoGratorTester: annoGratorTester.o ${MYLIBS}
	@${MKDIR} ${BIN_DIR}
	${CC} ${COPT} ${CFLAGS} -o $@ $< ${MYLIBS} ${L}


clean:
	rm -rf *.o bin output *.tmp loader udcCache
