include ../../inc/common.mk


O = scrambleFa.o 

scrambleFa: $O
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/scrambleFa $O ../../lib/$(MACHTYPE)/jkweb.a 

clean::
	rm -f ${O}
