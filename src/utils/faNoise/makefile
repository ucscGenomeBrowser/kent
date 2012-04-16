include ../../inc/common.mk


O = faNoise.o 

faNoise: $(O)
	${CC} ${COPT} ${CFLAGS} -lm -o ${BINDIR}/faNoise $O ../../lib/$(MACHTYPE)/jkweb.a ${L}

clean::
	rm -f ${O}
