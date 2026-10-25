kentSrc = ../../..
include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS = ${MYLIBDIR}/jkhgapcgi.a ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a
BIN_DIR = bin/${MACHTYPE}

test: backupParseTest archiveTrackLineTest
	rm -r output
	@echo tested all

mkdirs:
	@${MKDIR} output ${BIN_DIR}

# Reads the database settings from the developer's own ~/.hg.conf.
backupParseTest: mkdirs
	${CC} ${CC_PROG_OPTS} -o ${BIN_DIR}/backupParseTest backupParseTest.c ../backup.c ${MYLIBS} $L
	${BIN_DIR}/backupParseTest > output/backupParseTest
	diff expected/backupParseTest output/backupParseTest

# Needs no database and no hg.conf.
archiveTrackLineTest: mkdirs
	${CC} ${CC_PROG_OPTS} -o ${BIN_DIR}/archiveTrackLineTest archiveTrackLineTest.c ../backup.c ${MYLIBS} $L
	${BIN_DIR}/archiveTrackLineTest > output/archiveTrackLineTest
	diff expected/archiveTrackLineTest output/archiveTrackLineTest

clean:
	rm -rf *.o bin output
