include ../../../inc/common.mk

HG_WARN = ${HG_WARN_ERR}
AUTOSQL = ${HOME}/bin/${MACHTYPE}/autoSql
DIFF = diff -u

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

test:   hardTest.as  newTest.as  polyTest.as  mainTest.as \
        testHarness dbLinkTest

# hardTest
hardTest.as: mkout
	${AUTOSQL} input/hardTest.as output/hardTest
	${DIFF} output/hardTest.sql expected/hardTest.sql
	${DIFF} output/hardTest.h   expected/hardTest.h
	${DIFF} output/hardTest.c   expected/hardTest.c

# newTest
newTest.as: mkout
	${AUTOSQL} -dbLink input/newTest.as output/newTest
	${DIFF} output/newTest.sql expected/newTest.sql
	${DIFF} output/newTest.h   expected/newTest.h
	${DIFF} output/newTest.c   expected/newTest.c
	sed -e "s/output\\///" output/newTest.c > newTest.c
	sed -e "s/OUTPUT\\///" output/newTest.h > newTest.h
	cp output/newTest.sql .

# polyTest
polyTest.as: mkout
	${AUTOSQL} input/polyTest.as output/polyTest
	${DIFF} output/polyTest.sql expected/polyTest.sql
	${DIFF} output/polyTest.h   expected/polyTest.h
	${DIFF} output/polyTest.c   expected/polyTest.c

# test
mainTest.as: mkout
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

mainTest.c: mainTest.as

# dbLinkTest:
dbLinkTest:  newTest.o dbLinkTest.o
	${CC} -o dbLinkTest dbLinkTest.o newTest.o ${MYLIBS} ${MYSQLLIBS} ${L}
	./dbLinkTest >& output/dbLinkTest.out
	${DIFF} output/dbLinkTest.out expected/dbLinkTest.out

newTest.c: newTest.as

mkout:
	@${MKDIR} output

clean:
	rm -rf output *.o {mainTest,newTest}.[ch] newTest.sql \
          testHarness dbLinkTest
