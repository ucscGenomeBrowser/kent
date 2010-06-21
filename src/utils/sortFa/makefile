include ../../inc/common.mk


O = sortFa.o 

sortFa: $O
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/sortFa $O ../../lib/$(MACHTYPE)/jkweb.a 

clean::
	rm -f ${O}
