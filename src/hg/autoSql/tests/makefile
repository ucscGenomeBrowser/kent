include ../../../inc/common.mk

HG_WARN = ${HG_WARN_ERR}
AUTOSQL = ${HOME}/bin/${MACHTYPE}/autoSql
DIFF = diff -u

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

# .as not used as dependencies, as we want to run everything each time
test:   hardTest  newTest  polyTest  mainTest \
        testHarness dbLinkTest

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
	sed -e "s/output\\///" output/newTest.c > newTest.c
	sed -e "s/OUTPUT\\///" output/newTest.h > newTest.h
	cp output/newTest.sql .

# polyTest
polyTest: mkout
	${AUTOSQL} input/polyTest.as output/polyTest
	${DIFF} output/polyTest.sql expected/polyTest.sql
	${DIFF} output/polyTest.h   expected/polyTest.h
	${DIFF} output/polyTest.c   expected/polyTest.c

# test
mainTest: mkout
	${AUTOSQL} input/mainTest.as output/mainTest
	${DIFF} output/mainTest.sql expected/mainTest.sql
	${DIFF} output/mainTest.h   expected/mainTest.h
	${DIFF} output/mainTest.c   expected/mainTest.c
	sed -e "s/output\\///" output/mainTest.c > mainTest.c
	sed -e "s/OUTPUT\\///" output/mainTest.h > mainTest.h

# testHarness
testHarness: mainTest.o testHarness.o
	${CC} -o testHarness testHarness.o mainTest.o ${MYLIBS} ${MYSQLLIBS}
	./testHarness > output/testHarness.out
	${DIFF} output/testHarness.out expected/testHarness.out

mainTest.c: mainTest

# dbLinkTest:
dbLinkTest:  newTest.o dbLinkTest.o
	${CC} -o dbLinkTest dbLinkTest.o newTest.o ${MYLIBS} ${MYSQLLIBS} ${L}
	./dbLinkTest >& output/dbLinkTest.out
	${DIFF} output/dbLinkTest.out expected/dbLinkTest.out

newTest.c: newTest

mkout:
	@${MKDIR} output

clean:
	rm -rf output *.o {mainTest,newTest}.[ch] newTest.sql \
          testHarness dbLinkTest
