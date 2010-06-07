include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = mafFrags.o

mafFrags: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/mafFrags $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/mafFrags${EXE}

test:
	${MKDIR} output
	mafFrags hg16 humorMm3Rn3 input/hg16.bed output/hg16.maf -orgs=input/hg16.org
	diff -x CVS -r expected output
	rm -r output


clean::
	rm -fr output ${O} mafFrags
