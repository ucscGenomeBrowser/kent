include ../../inc/common.mk


O = uniqSize.o 

uniqSize: $(O)
	${CC} ${COPT} ${CFLAGS} -o uniqSize $O ../../lib/$(MACHTYPE)/jkweb.a ${L}

clean::
	rm -f ${O}
