include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = xmlToSql.o elStat.o rename.o tables.o

xmlToSql: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/xmlToSql $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/xmlToSql${EXE}

clean:
	rm -f $O
	rm -rf test/*/out

test::
	./doTest
