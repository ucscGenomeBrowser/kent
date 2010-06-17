include ../../inc/common.mk


O = unwrap.o 

unwrap: $(O)
	${CC} ${COPT} ${CFLAGS} -o unwrap $O ../../lib/$(MACHTYPE)/jkweb.a

clean::
	rm -f ${O}
