include ../../inc/common.mk


O = newProg.o 

newProg: $(O)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/newProg $O ../../lib/$(MACHTYPE)/jkweb.a ${L}

clean::
	rm -f ${O} newProg
