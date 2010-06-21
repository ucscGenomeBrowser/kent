include ../../inc/common.mk


O = undupFa.o 

undupFa: $(O)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/undupFa $O ../../lib/$(MACHTYPE)/jkweb.a

clean::
	rm -f ${O}
