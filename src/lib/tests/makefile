include ../../inc/common.mk
HG_WARN_ERR = -DJK_WARN -Wall -Werror

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a
BIN_DIR = bin/${MACHTYPE}

test: htmlPageTest
	rm -r output
	echo tested all

mkdirs:
	${MKDIR} output ${BIN_DIR}

htmlPageTest: htmlPageTest.o ${MYLIBS} mkdirs
	${CC} ${COPT} -o ${BIN_DIR}/htmlPageTest htmlPageTest.o ${MYLIBS} $L
	${BIN_DIR}/htmlPageTest >& output/htmlPageTest
	diff expected/htmlPageTest output/htmlPageTest

clean:
	rm -rf *.o bin output *.tmp
