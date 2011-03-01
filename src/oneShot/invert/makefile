include ../../inc/common.mk


O = invert.o 

invert: $(O)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/invert $O ../../lib/$(MACHTYPE)/jkweb.a ${L}

clean::
	rm -f ${O}
