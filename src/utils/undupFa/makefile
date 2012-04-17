include ../../inc/common.mk


O = undupFa.o 

undupFa: $(O)
	${CC} ${COPT} ${CFLAGS} -lm -o ${BINDIR}/undupFa $O ../../lib/$(MACHTYPE)/jkweb.a ${L}

clean::
	rm -f ${O}
