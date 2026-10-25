kentSrc = ../../..
include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS = ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a
BIN_DIR = bin/${MACHTYPE}

test: activationTokenTest
	rm -r output
	@echo tested all

mkdirs:
	@${MKDIR} output ${BIN_DIR}

# Writes gbMembers rows to hgcentralregress, so central.user needs UPDATE and DELETE there;
# see hg/lib/tests/makeHgCentralRegress.sh, which this runs first.
activationTokenTest: mkdirs
	${CC} ${CC_PROG_OPTS} -I.. -o ${BIN_DIR}/activationTokenTester activationTokenTester.c ../gbMembers.c ${MYLIBS} $L
	../../lib/tests/makeHgCentralRegress.sh > /dev/null
	echo "include ${HOME}/.hg.conf" > output/regress.conf
	echo "central.db=hgcentralregress" >> output/regress.conf
	HGDB_CONF=output/regress.conf ${BIN_DIR}/activationTokenTester > output/activationTokenTest
	diff expected/activationTokenTest output/activationTokenTest

clean:
	rm -rf bin output
