include ../../inc/common.mk


O = moresyn.o 
A = moresyn

moresyn: ${O}
	${CC} -lm -o ${BINDIR}/$A $O ../../lib/${MACHTYPE}/jkweb.a ${L}
	${STRIP} ${BINDIR}/$A${EXE}

clean:
	rm -f ${O}
