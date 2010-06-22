include ../../inc/common.mk


O = faNoise.o 

faNoise: $(O)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/faNoise $O ../../lib/$(MACHTYPE)/jkweb.a

clean::
	rm -f ${O}
