include ../../inc/common.mk


O = fsg.o 

${BINDIR}/fsg: $(O)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/fsg $O ../../lib/$(MACHTYPE)/jkweb.a ${L}

clean::
	rm -f ${O}
