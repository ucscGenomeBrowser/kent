include ../../inc/common.mk


O = undupFa.o 

undupFa: $(O)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/undupFa $O ../../lib/$(MACHTYPE)/jkweb.a ${L}

clean::
	rm -f ${O}
