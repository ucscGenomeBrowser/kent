include ../../inc/common.mk

O = simpleS.o

simpleS: $(O)
	${CC} ${COPT} ${CFLAGS} -o simpleS $O ../../lib/$(MACHTYPE)/jkweb.a

clean::
	rm -f ${O}
