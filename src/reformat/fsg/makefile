include ../../inc/common.mk


O = fsg.o 

${BINDIR}/fsg: $(O)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/fsg $O ../../lib/$(MACHTYPE)/jkweb.a

clean::
	rm -f ${O}
