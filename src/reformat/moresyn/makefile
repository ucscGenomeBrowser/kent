include ../../inc/common.mk


O = moresyn.o 
A = moresyn

moresyn: ${O}
	${CC} -o ${HOME}/bin/${MACHTYPE}/$A $O ../../lib/${MACHTYPE}/jkweb.a
	strip ${HOME}/bin/${MACHTYPE}/$A

clean:
	rm -f ${O}
