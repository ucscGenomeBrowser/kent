include ../../inc/common.mk


O = gbtofa.o 

gbtofa: $(O)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/gbtofa $O ../../lib/$(MACHTYPE)/jkweb.a

clean::
	rm -f ${O}
