include ../../inc/common.mk


O = stToXao.o 

stToXao: $(O)
	${CC} ${COPT} ${CFLAGS} -lm $O ../../lib/$(MACHTYPE)/jkweb.a $L
	mv ${AOUT} ${BINDIR}/stToXao

clean::
	rm -f ${O}
