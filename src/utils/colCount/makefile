include ../../inc/common.mk


O = colCount.o 

colCount: $O
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/colCount $O ../../lib/$(MACHTYPE)/jkweb.a ${L}

clean::
	rm -f ${O}
