include ../../inc/common.mk


O = subChar.o 

subChar: $O
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/subChar $O ../../lib/$(MACHTYPE)/jkweb.a ${L}
	${STRIP} ${BINDIR}/subChar${EXE}

clean::
	rm -f ${O}
