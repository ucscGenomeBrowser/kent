include ../../inc/common.mk


O = coverage.o 

coverage: $(O)
	${CC} ${COPT} ${CFLAGS} -o coverage $O ../../lib/$(MACHTYPE)/jkweb.a ${L}

clean::
	rm -f ${O}
