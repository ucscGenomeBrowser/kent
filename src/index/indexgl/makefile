include ../../inc/common.mk


O = indexgl.o 

indexgl: ${O} 
	${CC} $O ../../lib/${MACHTYPE}/jkweb.a \
		-o ${BINDIR}/indexgl
	${STRIP} ${BINDIR}/indexgl${EXE}

lib:
	cd ../../lib && ${MAKE}

