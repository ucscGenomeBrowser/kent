include ../../inc/common.mk
HG_WARN_ERR = -DJK_WARN -Wall -Werror

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a
BIN_DIR = bin/${MACHTYPE}

test: errCatchTest htmlPageTest htmlExpandUrlTest
	rm -r output
	echo tested all

mkdirs:
	${MKDIR} output ${BIN_DIR}

errCatchTest: errCatchTest.o ${MYLIBS} mkdirs
	${CC} ${COPT} -o ${BIN_DIR}/errCatchTest errCatchTest.o ${MYLIBS} $L
	${STRIP} ${BIN_DIR}/errCatchTest${EXE}
	${BIN_DIR}/errCatchTest secret > output/errCatch.good
	diff expected/errCatch.good output/errCatch.good
	${BIN_DIR}/errCatchTest bad > output/errCatch.bad
	diff expected/errCatch.bad output/errCatch.bad

htmlExpandUrlTest: htmlExpandUrlTest.o ${MYLIBS} mkdirs
	${CC} ${COPT} -o ${BIN_DIR}/htmlExpandUrlTest htmlExpandUrlTest.o ${MYLIBS} $L
	${STRIP} ${BIN_DIR}/htmlExpandUrlTest${EXE}
	${BIN_DIR}/htmlExpandUrlTest > output/htmlExpandUrlTest 2>&1
	diff expected/htmlExpandUrlTest output/htmlExpandUrlTest

htmlPageTest: htmlPageTest.o ${MYLIBS} mkdirs
	${CC} ${COPT} -o ${BIN_DIR}/htmlPageTest htmlPageTest.o ${MYLIBS} $L
	${STRIP} ${BIN_DIR}/htmlPageTest${EXE}
	${BIN_DIR}/htmlPageTest input/google.html > output/google.out
	diff expected/google.out output/google.out

clean:
	rm -rf *.o bin output *.tmp
