include ../../inc/common.mk


O = addCols.o 

addCols: $O
	${CC} ${COPT} ${CFLAGS} -lm -o ${BINDIR}/addCols $O ../../lib/$(MACHTYPE)/jkweb.a ${L}

clean::
	rm -f ${O}
