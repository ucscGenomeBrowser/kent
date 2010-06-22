include ../../inc/common.mk


O = addCols.o 

addCols: $O
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/addCols $O ../../lib/$(MACHTYPE)/jkweb.a 

clean::
	rm -f ${O}
