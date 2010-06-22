include ../../inc/common.mk


O = wordLine.o 

wordLine: $O
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/wordLine $O ../../lib/$(MACHTYPE)/jkweb.a 
	${STRIP} ${BINDIR}/wordLine${EXE}

clean::
	rm -f ${O}
