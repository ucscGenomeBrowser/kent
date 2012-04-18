include ../../inc/common.mk


O = upper.o 

upper: $(O)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/upper $O ../../lib/$(MACHTYPE)/jkweb.a ${L}

clean::
	rm -f ${O}
