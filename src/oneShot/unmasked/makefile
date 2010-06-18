include ../../inc/common.mk


O = unmasked.o 

unmasked: $(O)
	${CC} ${COPT} ${CFLAGS} -o unmasked $O ../../lib/$(MACHTYPE)/jkweb.a

clean::
	rm -f ${O}
