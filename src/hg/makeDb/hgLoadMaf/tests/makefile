include ../../../../inc/common.mk
# tests for hgLoadMaf

PROG = hgLoadMaf
P = ${PROG}

test: testoutput clean

TEST1 = testNan

testoutput: ${TEST1}

clean:
	rm -fr output

testNan: init-tests
	$P -warn -test=input/${TEST1}.maf hg16 ${TEST1} > output/${TEST1}.log
	mv ${TEST1}.tab output
	diff output/${TEST1}.tab expected
	diff output/${TEST1}.log expected

init-tests:
	${MKDIR} output

