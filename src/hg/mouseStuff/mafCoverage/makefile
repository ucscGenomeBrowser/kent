include ../../../inc/common.mk

L = -lm ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = mafCoverage.o

mafCoverage: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/mafCoverage $O ${MYLIBS} $L
#	${STRIP} ${HOME}/bin/${MACHTYPE}/mafCoverage${EXE}

test:	test1 test2
%:	input/%.maf
	${MKDIR} output
	mafCoverage hg15 input/$@.maf > output/$@.txt
	diff output/$@.txt expected/$@.txt
	rm output/$@.txt

clean::
	rm -fr output ${O}
