include ../../inc/common.mk

HG_WARN=-Wall

L = -lm ${MYSQLLIBS}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = randomPlacement

O = randomPlacement.o

${A}:	${O}
	${CC} -o ${A}${EXE} -g $O ${MYLIBS} ${L} 

compile: $O 
	${CC} -g $O ${MYLIBS} ${L} 
	mv ${AOUT} $A${EXE}

clean::
	rm -f ${O} ${A}${EXE} ${AOUT}
