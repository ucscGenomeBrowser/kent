include ../../inc/common.mk


O = wordLine.o 

wordLine: $O
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/wordLine $O ../../lib/$(MACHTYPE)/jkweb.a ${L}
	${STRIP} ${BINDIR}/wordLine${EXE}

clean::
	rm -f ${O}
