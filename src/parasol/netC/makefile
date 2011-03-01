include ../../inc/common.mk

O = netC.o

netC: $(O)
	${CC} ${COPT} ${CFLAGS} -o netC $O ../../lib/$(MACHTYPE)/jkweb.a ${L}

clean::
	rm -f ${O}
