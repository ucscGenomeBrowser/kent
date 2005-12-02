include ../../inc/common.mk


O = sizeof.o

sizeof: $O
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/sizeof${EXE} $O
	${STRIP} $(HOME)/bin/$(MACHTYPE)/sizeof${EXE}

