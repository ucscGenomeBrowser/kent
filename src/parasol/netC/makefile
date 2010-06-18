include ../../inc/common.mk

O = netC.o

netC: $(O)
	${CC} ${COPT} ${CFLAGS} -o netC $O ../../lib/$(MACHTYPE)/jkweb.a

clean::
	rm -f ${O}
