include ../../inc/common.mk


O = scrambleFa.o 

scrambleFa: $O
	${CC} ${COPT} ${CFLAGS} -lm -o ${BINDIR}/scrambleFa $O ../../lib/$(MACHTYPE)/jkweb.a ${L}

clean::
	rm -f ${O}
