include ../../inc/common.mk


O = cCp.o 

cCp: $O
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/cCp $O ../../lib/$(MACHTYPE)/jkweb.a ${L}

clean::
	rm -f ${O}
