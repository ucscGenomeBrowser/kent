include ../../inc/common.mk


O = aveCols.o 

aveCols: $O
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/aveCols $O ../../lib/$(MACHTYPE)/jkweb.a ${L}

clean::
	rm -f ${O}
