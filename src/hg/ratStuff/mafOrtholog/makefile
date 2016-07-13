kentSrc = ../../..
include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = mafOrtholog.o

mafOrtholog: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/mafOrtholog $O ${MYLIBS} $L
#	${STRIP} ${BINDIR}/mafOrtholog${EXE}

test:
	${MKDIR} output
	mafOrtholog hg18 multiz6waySyn input/test.gp output/test.tab 
	diff -x CVS -r expected output
	rm -r output


clean::
	rm -fr output ${O} mafOrtholog
