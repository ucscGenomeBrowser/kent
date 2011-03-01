include ../../inc/common.mk

O = simpleS.o

simpleS: $(O)
	${CC} ${COPT} ${CFLAGS} -o simpleS $O ../../lib/$(MACHTYPE)/jkweb.a ${L}

clean::
	rm -f ${O}
