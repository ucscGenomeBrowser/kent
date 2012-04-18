include ../../inc/common.mk


O = fakeOut.o 

fakeOut: $(O)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/fakeOut $O ../../lib/$(MACHTYPE)/jkweb.a ${L}

clean::
	rm -f ${O}
