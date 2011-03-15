include ../../inc/common.mk


O = countChars.o 

countChars: $O
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/countChars $O ../../lib/$(MACHTYPE)/jkweb.a ${L}
	${STRIP} ${BINDIR}/countChars${EXE}

clean::
	rm -f ${O}
