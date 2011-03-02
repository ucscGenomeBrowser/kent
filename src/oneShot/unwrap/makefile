include ../../inc/common.mk


O = unwrap.o 

unwrap: $(O)
	${CC} ${COPT} ${CFLAGS} -o unwrap $O ../../lib/$(MACHTYPE)/jkweb.a ${L}

clean::
	rm -f ${O}
