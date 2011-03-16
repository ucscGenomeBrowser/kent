include ../../inc/common.mk


O = sources.o 

${BINDIR}/sources: $(O)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/sources $O ../../lib/$(MACHTYPE)/jkweb.a ${L}

clean::
	rm -f ${O}
