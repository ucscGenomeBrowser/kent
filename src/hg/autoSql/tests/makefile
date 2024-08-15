kentSrc = ../../..
include ../../../inc/common.mk

#This was making warnings not happen
#HG_WARN = ${HG_WARN_ERR}

AUTOSQL = ${DESTBINDIR}/autoSql
AUTOSQLJSON = ${AUTOSQL} -json
DIFF = diff -u
XINC = -I.

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

# .as not used as dependencies, as we want to run everything each time
test:   hardTest  newTest  polyTest  simpleTest mainTest doc doc2 \
        testHarness dbLinkTest symTest symColsTest jsonTest

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

# doc
doc: mkout
	${AUTOSQLJSON} input/doc.as output/doc
	${DIFF} expected/doc.sql output/doc.sql
	${DIFF} expected/doc.h   output/doc.h  
	${DIFF} expected/doc.c   output/doc.c  

# doc2
doc2: mkout
	${AUTOSQLJSON} input/doc2.as output/doc2
	${DIFF} expected/doc2.sql output/doc2.sql
	${DIFF} expected/doc2.h   output/doc2.h  
	${DIFF} expected/doc2.c   output/doc2.c  

# index
index: mkout
	${AUTOSQL} input/index.as output/index
	${DIFF} expected/index.sql output/index.sql

# bin
binTest: mkout
	${AUTOSQL} -addBin input/binTest.as output/binTest
	${DIFF} expected/binTest.sql output/binTest.sql
	${DIFF} expected/binTest.c output/binTest.c
	${DIFF} expected/binTest.h output/binTest.h


# testHarness
testHarness: output/mainTest.c
	${CC} ${CC_PROG_OPTS} -o testHarness testHarness.c output/mainTest.c ${MYLIBS} ${MYSQLLIBS} ${L}
	./testHarness > output/testHarness.out
	${DIFF} expected/testHarness.out output/testHarness.out

output/mainTest.c: mainTest

# dbLinkTest:
dbLinkTest:  output/newTest.c
	${CC} ${CC_PROG_OPTS} -o dbLinkTest dbLinkTest.c output/newTest.c ${MYLIBS} ${MYSQLLIBS} ${L}
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

symColsTest: output/symTest.c
	${CC} ${CC_PROG_OPTS} -o testSymCols testSymCols.c output/symTest.c ${MYLIBS} ${MYSQLLIBS} ${L}
	./testSymCols
	${DIFF} expected/symColsTest.tab output/symColsTest.tab
	${DIFF} expected/symColsTest.tab output/symColsTestComma.tab
	${DIFF} expected/symColsTest.tab output/symColsTestDb.tab
	rm testSymCols

output/symTest.c: symTest

# test of JSON output
jsonTest: output/doc.c output/doc2.c
	${CC} ${CC_PROG_OPTS} -o jsonTest jsonTest.c output/doc.c output/doc2.c ${MYLIBS} ${MYSQLLIBS} ${L}
	./jsonTest >& output/jsonTest.out
	${DIFF} expected/jsonTest.out output/jsonTest.out

output/doc.c: doc
output/doc2.c: doc2

mkout:
	@${MKDIR} output

clean:
	rm -rf output *.o testHarness dbLinkTest jsonTest
