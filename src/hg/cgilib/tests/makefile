kentSrc = ../../..
include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgapcgi.a ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a
BIN_DIR = bin/${MACHTYPE}

all: 	${BIN_DIR}/annoGratorTester 

#test: annoGratorTest 
test: 
	rm -r output
	echo tested all

mkdirs:
	@${MKDIR} output ${BIN_DIR}

annoGratorTest: ${BIN_DIR}/annoGratorTester mkdirs
	${MAKE} -f annoGratorTests.mk test

${BIN_DIR}/annoGratorTester: annoGratorTester.o ${MYLIBS}
	@${MKDIR} ${BIN_DIR}
	${CC} ${COPT} ${CFLAGS} -o $@ $< ${MYLIBS} ${L}


clean:
	rm -rf *.o bin output *.tmp loader udcCache
