include ../../inc/common.mk

O = netS.o

netS: $(O)
	${CC} ${COPT} ${CFLAGS} -o netS $O ../../lib/$(MACHTYPE)/jkweb.a ${L}

clean::
	rm -f ${O}
