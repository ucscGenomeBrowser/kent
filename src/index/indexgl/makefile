include ../../inc/common.mk


O = indexgl.o 

indexgl: ${O} 
	${CC} $O ../../lib/${MACHTYPE}/jkweb.a \
		-o ${HOME}/bin/${MACHTYPE}/indexgl
	${STRIP} ${HOME}/bin/${MACHTYPE}/indexgl${EXE}

lib:
	cd ../../lib && ${MAKE}

