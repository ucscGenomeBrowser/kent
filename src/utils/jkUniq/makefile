include ../../inc/common.mk


O = jkUniq.o 

jkUniq: $O
	${CC} ${COPT} ${CFLAGS} -lm -o ${BINDIR}/jkUniq $O ../../lib/$(MACHTYPE)/jkweb.a ${L}

clean::
	rm -f ${O}
