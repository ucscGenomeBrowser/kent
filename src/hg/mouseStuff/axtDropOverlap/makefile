include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a


O = axtDropOverlap.o 

axtDropOverlap: $O ${MYLIBS}
	${CC} -o ${HOME}/bin/${MACHTYPE}/axtDropOverlap $O ${MYLIBS} $L 

lib:
	cd ../../lib && ${MAKE}

test:	test1 test2
%:	input/%.axt
	${MKDIR} output
	axtDropOverlap input/$@.axt input/H input/H output/$@.axt
	diff output/$@.axt expected/$@.axt
	rm output/$@.axt

clean::
	rm -fr output ${O}
