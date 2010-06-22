include ../../inc/common.mk


O = sizeof.o

sizeof: $O
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/sizeof${EXE} $O
	${STRIP} ${BINDIR}/sizeof${EXE}

clean::
	rm -f ${O}
