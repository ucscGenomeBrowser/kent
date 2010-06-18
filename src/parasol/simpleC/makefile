include ../../inc/common.mk

O = simpleC.o

simpleC: $(O)
	${CC} ${COPT} ${CFLAGS} -o simpleC $O ../../lib/$(MACHTYPE)/jkweb.a

clean::
	rm -f ${O}
