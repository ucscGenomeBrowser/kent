include ../../inc/common.mk


O = rmFaDups.o 

rmFaDups: $(O)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/rmFaDups $O ../../lib/$(MACHTYPE)/jkweb.a ${L}

clean::
	rm -f ${O}
