include ../../inc/common.mk


O = jkUniq.o 

jkUniq: $O
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/jkUniq $O ../../lib/$(MACHTYPE)/jkweb.a ${L}

clean::
	rm -f ${O}
