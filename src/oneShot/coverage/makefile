include ../../inc/common.mk


O = coverage.o 

coverage: $(O)
	${CC} ${COPT} ${CFLAGS} -o coverage $O ../../lib/$(MACHTYPE)/jkweb.a

clean::
	rm -f ${O}
