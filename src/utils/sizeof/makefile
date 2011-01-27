include ../../inc/common.mk


O = sizeof.o

sizeof: $O
	${CC} ${COPT} ${CFLAGS} -o ${DESTDIR}${BINDIR}/sizeof${EXE} $O
	${STRIP} ${DESTDIR}${BINDIR}/sizeof${EXE}

clean::
	rm -f ${O} sizeof${EXE}

compile: $O
	${CC} ${COPT} ${CFLAGS} -o sizeof${EXE} $O
