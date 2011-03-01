include ../../inc/common.mk


O = shorten.o 

shorten: $(O)
	${CC} ${COPT} ${CFLAGS} $O -o ${BINDIR}/shorten ../../lib/$(MACHTYPE)/jkweb.a ${L}

clean::
	rm -f ${O}
