include ../../inc/common.mk

O = netS.o

netS: $(O)
	${CC} ${COPT} ${CFLAGS} -o netS $O ../../lib/$(MACHTYPE)/jkweb.a

clean::
	rm -f ${O}
