include ../../../inc/common.mk

HG_WARN = ${HG_WARN_ERR}
AUTOSQL = ${BINDIR}/autoSql -noRcsIds
DIFF = diff -u
XINC = -I.

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

# .as not used as dependencies, as we want to run everything each time
test:   hardTest  newTest  polyTest  simpleTest mainTest \
        testHarness dbLinkTest symTest symColsTest

# hardTest
hardTest: mkout
	${AUTOSQL} input/hardTest.as output/hardTest
	${DIFF} expected/hardTest.sql output/hardTest.sql
	${DIFF} expected/hardTest.h   output/hardTest.h  
	${DIFF} expected/hardTest.c   output/hardTest.c  

# newTest
newTest: mkout
	${AUTOSQL} -dbLink input/newTest.as output/newTest 2>output/newTest.err
	${DIFF} expected/newTest.sql output/newTest.sql
	${DIFF} expected/newTest.h   output/newTest.h  
	${DIFF} expected/newTest.c   output/newTest.c  
	${DIFF} expected/newTest.err   output/newTest.err

# polyTest
polyTest: mkout
	${AUTOSQL} input/polyTest.as output/polyTest
	${DIFF} expected/polyTest.sql output/polyTest.sql
	${DIFF} expected/polyTest.h   output/polyTest.h  
	${DIFF} expected/polyTest.c   output/polyTest.c  

# simpleTest
simpleTest: mkout
	${AUTOSQL} input/simpleTest.as output/simpleTest
	${DIFF} expected/simpleTest.sql output/simpleTest.sql
	${DIFF} expected/simpleTest.h   output/simpleTest.h  
	${DIFF} expected/simpleTest.c   output/simpleTest.c  

# test
mainTest: mkout
	${AUTOSQL} input/mainTest.as output/mainTest
	${DIFF} expected/mainTest.sql output/mainTest.sql
	${DIFF} expected/mainTest.h   output/mainTest.h  
	${DIFF} expected/mainTest.c   output/mainTest.c  

# testHarness
testHarness: output/mainTest.o output/simpleTest.o testHarness.o
	${CC} -o testHarness testHarness.o output/mainTest.o ${MYLIBS} ${MYSQLLIBS}
	./testHarness > output/testHarness.out
	${DIFF} expected/testHarness.out output/testHarness.out

output/mainTest.c: mainTest

# dbLinkTest:
dbLinkTest:  output/newTest.o dbLinkTest.o
	${CC} -o dbLinkTest dbLinkTest.o output/newTest.o ${MYLIBS} ${MYSQLLIBS} ${L}
	./dbLinkTest >& output/dbLinkTest.out
	${DIFF} expected/dbLinkTest.out output/dbLinkTest.out

output/newTest.c: newTest

# test of symbolic columns

symTest: mkout
	${AUTOSQL} input/symTest.as output/symTest 2>output/symTest.err
	${DIFF} expected/symTest.sql output/symTest.sql
	${DIFF} expected/symTest.h   output/symTest.h  
	${DIFF} expected/symTest.c   output/symTest.c  
	${DIFF} expected/symTest.err   output/symTest.err

output/symTest.c: symTest

symColsTest: testSymCols.o output/symTest.o
	${CC} -o testSymCols testSymCols.o output/symTest.o ${MYLIBS} ${MYSQLLIBS} ${L}
	./testSymCols
	${DIFF} expected/symColsTest.tab output/symColsTest.tab
	${DIFF} expected/symColsTest.tab output/symColsTestComma.tab
	${DIFF} expected/symColsTest.tab output/symColsTestDb.tab
	rm testSymCols

testSymCols.o: output/symTest.c

mkout:
	@${MKDIR} output

clean:
	rm -rf output *.o testHarness dbLinkTest
