include ../../inc/common.mk


O = aveCols.o 

aveCols: $O
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/aveCols $O ../../lib/$(MACHTYPE)/jkweb.a 

clean::
	rm -f ${O}
