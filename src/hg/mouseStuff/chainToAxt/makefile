include ../../../inc/common.mk

L = -lm
MYLIBDIR = ${HOME}/src/lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = chainToAxt.o

chainToAxt: $O ${MYLIBS}
	gcc -ggdb -o ${HOME}/bin/${MACHTYPE}/chainToAxt $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/chainToAxt${EXE}

test: chrM
%:	input/%.chain
	${MKDIR} output
	chainToAxt input/$@.chain /gbdb/hg16/nib /gbdb/mm3/nib output/$@.axt
	diff output/$@.axt expected/$@.axt
	rm output/$@.axt

clean::
	rm -fr output ${O}
