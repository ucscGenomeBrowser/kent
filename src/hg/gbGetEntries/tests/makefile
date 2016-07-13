kentSrc = ../../..
include ../../../inc/common.mk

gbGetEntries=${DESTBINDIR}/gbGetEntries

all:

test: noVerCmdTest noVerFileTest noVerMissingTest noVerMissingOkTest \
	verCmdTest verFileTest verMissingTest verMissingOkTest accDupTest

# test no version on cmdline
noVerCmdTest: mkdirs
	${gbGetEntries} input/test.gbff NM_009304 >output/$@.gbff
	(wc output/$@.gbff && fgrep VERSION output/$@.gbff) >output/$@.info
	diff -bu expected/$@.info output/$@.info

# test no-version acc in file, one has two diff version in gbff
noVerFileTest: mkdirs
	${gbGetEntries} -accFile=input/noVer.acc input/test.gbff >output/$@.gbff
	(wc output/$@.gbff && fgrep VERSION output/$@.gbff) >output/$@.info
	diff -bu expected/$@.info output/$@.info

# test no version and one missing
noVerMissingTest: mkdirs
	${gbGetEntries} input/test.gbff NM_009304 NM_004666 >output/$@.gbff 2>output/$@.info || true
	(wc output/$@.gbff && fgrep VERSION output/$@.gbff) >>output/$@.info
	diff -bu expected/$@.info output/$@.info


# test no version, one missing, missing ok
noVerMissingOkTest: mkdirs
	${gbGetEntries} -missingOk input/test.gbff NM_009304 NM_004666 >output/$@.gbff
	(wc output/$@.gbff && fgrep VERSION output/$@.gbff) >output/$@.info
	diff -bu expected/$@.info output/$@.info

# test version on cmdline
verCmdTest: mkdirs
	${gbGetEntries} input/test.gbff NM_009304 >output/$@.gbff
	(wc output/$@.gbff && fgrep VERSION output/$@.gbff) >output/$@.info
	diff -bu expected/$@.info output/$@.info

# test-version acc in file, one has two diff version in gbff
verFileTest: mkdirs
	${gbGetEntries} -accFile=input/ver.acc input/test.gbff >output/$@.gbff
	(wc output/$@.gbff && fgrep VERSION output/$@.gbff) >output/$@.info
	diff -bu expected/$@.info output/$@.info

# test version and one missing, one diff version
verMissingTest: mkdirs
	${gbGetEntries} input/test.gbff NM_009304.1 NM_004236.3 NM_004666.1 >output/$@.gbff 2>output/$@.info || true
	(wc output/$@.gbff && fgrep VERSION output/$@.gbff) >>output/$@.info
	diff -bu expected/$@.info output/$@.info

# test version, one missing, missing ok
verMissingOkTest: mkdirs
	${gbGetEntries} -missingOk input/test.gbff NM_009304 NM_004666 >output/$@.gbff
	(wc output/$@.gbff && fgrep VERSION output/$@.gbff) >output/$@.info
	diff -bu expected/$@.info output/$@.info

# test that duplicate entries in acc file are 
accDupTest: mkdirs
	${gbGetEntries} -accFile=input/dup.acc input/test.gbff >output/$@.gbff
	(wc output/$@.gbff && fgrep VERSION output/$@.gbff) >output/$@.info
	diff -bu expected/$@.info output/$@.info

mkdirs:
	@${MKDIR} output

clean::
	rm -fr output
