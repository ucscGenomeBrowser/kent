kentSrc = ../../../..
include ../../../../inc/common.mk
# tests for hgLoadMaf

hgLoadMaf = ${DESTBINDIR}/hgLoadMaf
hgLoadMafSummary = ${DESTBINDIR}/hgLoadMafSummary

#test: testoutput clean
test: testdot testpipe clean

TEST1 = testNan

testoutput: ${TEST1}

clean:
	rm -fr output

testdot: init-tests
	${hgLoadMafSummary} -test hg38 output/testDot input/testDot.maf
	diff output/testDot.tab expected

testpipe: init-tests
	${hgLoadMafSummary} -test hg38 output/testPipe input/testPipe.maf
	diff output/testPipe.tab expected

testNan: init-tests
	${hgLoadMaf} -warn -test=input/${TEST1}.maf hg16 ${TEST1} > output/${TEST1}.log
	mv ${TEST1}.tab output
	diff output/${TEST1}.tab expected
	diff output/${TEST1}.log expected

init-tests:
	${MKDIR} output

