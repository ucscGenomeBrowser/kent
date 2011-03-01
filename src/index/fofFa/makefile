include ../../inc/common.mk


O = fofFa.o 

fofFa: $(O)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/fofFa $O ../../lib/$(MACHTYPE)/jkweb.a ${L}

clean::
	rm -f ${O}
