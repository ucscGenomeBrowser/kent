include ../../inc/common.mk


O = fa4blast.o 

fa4blast: $(O)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/fa4blast $O ../../lib/$(MACHTYPE)/jkweb.a

clean::
	rm -f ${O}
