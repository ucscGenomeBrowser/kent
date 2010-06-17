include ../../inc/common.mk


O = shorten.o 

shorten: $(O)
	${CC} ${COPT} ${CFLAGS} $O -o ${BINDIR}/shorten ../../lib/$(MACHTYPE)/jkweb.a

clean::
	rm -f ${O}
