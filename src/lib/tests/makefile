include ../../inc/common.mk
HG_WARN_ERR = -DJK_WARN -Wall -Werror

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a
BIN_DIR = bin/${MACHTYPE}

test: htmlPageTest htmlExpandUrlTest
	rm -r output
	echo tested all

mkdirs:
	${MKDIR} output ${BIN_DIR}

htmlExpandUrlTest: htmlExpandUrlTest.o ${MYLIBS} mkdirs
	${CC} ${COPT} -o ${BIN_DIR}/htmlExpandUrlTest htmlExpandUrlTest.o ${MYLIBS} $L
	strip ${BIN_DIR}/htmlExpandUrlTest
	${BIN_DIR}/htmlExpandUrlTest >& output/htmlExpandUrlTest
	diff expected/htmlExpandUrlTest output/htmlExpandUrlTest

htmlPageTest: htmlPageTest.o ${MYLIBS} mkdirs
	${CC} ${COPT} -o ${BIN_DIR}/htmlPageTest htmlPageTest.o ${MYLIBS} $L
	strip ${BIN_DIR}/htmlPageTest
	${BIN_DIR}/htmlPageTest input/google.html > output/google.out
	diff expected/google.out output/google.out

clean:
	rm -rf *.o bin output *.tmp
