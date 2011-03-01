include ../../inc/common.mk


O = gbToFaRa.o 

gbToFaRa: $(O)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/gbToFaRa $O ../../lib/$(MACHTYPE)/jkweb.a ${L}

clean::
	rm -f ${O}
