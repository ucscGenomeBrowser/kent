include ../../inc/common.mk


O = moresyn.o 
A = moresyn

moresyn: ${O}
	${CC} -o ${HOME}/bin/${MACHTYPE}/$A $O ../../lib/${MACHTYPE}/jkweb.a
	${STRIP} ${HOME}/bin/${MACHTYPE}/$A${EXE}

clean:
	rm -f ${O}
