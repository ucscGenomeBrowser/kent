include ../../inc/common.mk


O = colCount.o 

colCount: $O
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/colCount $O ../../lib/$(MACHTYPE)/jkweb.a 

clean::
	rm -f ${O}
