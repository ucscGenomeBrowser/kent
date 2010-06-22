include ../../inc/common.mk


O = subs.o 

subs: $O
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/subs $O ../../lib/$(MACHTYPE)/jkweb.a 

clean::
	rm -f ${O}
