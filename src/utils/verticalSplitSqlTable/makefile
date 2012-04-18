include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

A = verticalSplitSqlTable
O = verticalSplitSqlTable.o

verticalSplitSqlTable: ${O} ${MYLIBS}
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS} $L
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

compile:: ${O}
	${CC} ${COPT} -o ${A}${EXE} ${O} ${MYLIBS} $L

clean::
	rm -f ${A}${EXE} ${O}

test:
	verticalSplitSqlTable -mergeOk tests/in/antibody.tab tests/in/antibody.as tests/in/antibody.split tests/out
	diff tests/expected/cvDb_abTarget.as tests/out/cvDb_abTarget.as
	diff tests/expected/cvDb_abTarget.tab tests/out/cvDb_abTarget.tab
	diff tests/expected/cvDb_antibody.as tests/out/cvDb_antibody.as
	diff tests/expected/cvDb_antibody.tab tests/out/cvDb_antibody.tab
	diff tests/expected/mergeErrs.txt tests/out/mergeErrs.txt
