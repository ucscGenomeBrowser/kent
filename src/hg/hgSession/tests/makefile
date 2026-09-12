kentSrc = ../../..
include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS = ${MYLIBDIR}/jkhgapcgi.a ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a
BIN_DIR = bin/${MACHTYPE}

test: backupParseTest
	rm -r output
	@echo tested all

mkdirs:
	@${MKDIR} output ${BIN_DIR}

# Run once with the hg.conf gate off and once with it on, so the test pins both
# the old behavior and the new one.  The generated conf includes the developer's
# own so the database settings come along.
backupParseTest: mkdirs
	${CC} ${CC_PROG_OPTS} -o ${BIN_DIR}/backupParseTest backupParseTest.c ../backup.c ${MYLIBS} $L
	echo "include ${HOME}/.hg.conf" > output/off.hg.conf
	chmod 600 output/off.hg.conf
	echo "include ${HOME}/.hg.conf" > output/on.hg.conf
	echo "skipMalformedCgiPairs=on" >> output/on.hg.conf
	chmod 600 output/on.hg.conf
	HGDB_CONF=output/off.hg.conf ${BIN_DIR}/backupParseTest > output/backupParseTest
	HGDB_CONF=output/on.hg.conf ${BIN_DIR}/backupParseTest >> output/backupParseTest
	diff expected/backupParseTest output/backupParseTest

clean:
	rm -rf *.o bin output
