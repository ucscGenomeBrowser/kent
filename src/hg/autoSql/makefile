include ../../inc/common.mk


L = -lm 
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a


O = autoSql.o

autoSql: ${O}
	${CC} -o autoSql $O ../../lib/${MACHTYPE}/jkweb.a ${L}
	cp autoSql ${HOME}/bin/${MACHTYPE}

test: test.o testHarness.o
	${CC} -o test testHarness.o test.o ${MYLIBS} ${MYSQLLIBS}

newTest.c: newTest.as 
	autoSql newTest.as newTest -dbLink

dbLinkTest:  newTest.o dbLinkTest.o
	${CC} -o dbLinkTest dbLinkTest.o newTest.o ${MYLIBS} ${MYSQLLIBS}

lib:
	cd ../../lib; make

clean::
	rm -f autoSql ${O} test.o testHarness.o test dbLinkTest.o dbLinkTest \
	newTest.c newTest.h newTest.o newTest.sql
