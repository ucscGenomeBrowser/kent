include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = spTest.o

spTest: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/spTest $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/spTest${EXE}

test: 
	${MKDIR} output
	spTest swissProt Q9FFH7 > output/Q9FFH7
	diff expected/Q9FFH7 output/Q9FFH7
	spTest swissProt P29312 > output/P29312
	diff expected/P29312 output/P29312
	rm -r output

clean::
	rm -fr output ${O}
