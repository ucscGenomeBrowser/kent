include ../../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a
BIN_DIR = bin/${MACHTYPE}

all: 	${BIN_DIR}/encodeExpTester

test: encodeExpTest
	rm -r output
	echo tested all

mkdirs:
	@${MKDIR} output ${BIN_DIR}

encodeExpTest: ${BIN_DIR}/encodeExpTester mkdirs
	${MAKE} -f encodeExpTests.mk test

${BIN_DIR}/encodeExpTester: encodeExpTester.o ${MYLIBS}
	@${MKDIR} ${BIN_DIR}
	${CC} ${COPT} ${CFLAGS} -o $@ $< ${MYLIBS} ${L}

clean:
	rm -rf *.o bin output
