include ../../inc/common.mk


O = gbToFaRa.o 

gbToFaRa: $(O)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/gbToFaRa $O ../../lib/$(MACHTYPE)/jkweb.a

clean::
	rm -f ${O}
