include ../../inc/common.mk


O = gbtofa.o 

gbtofa: $(O)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/gbtofa $O ../../lib/$(MACHTYPE)/jkweb.a ${L}

clean::
	rm -f ${O}
