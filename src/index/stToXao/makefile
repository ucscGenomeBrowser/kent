include ../../inc/common.mk


O = stToXao.o 

stToXao: $(O)
	${CC} ${COPT} ${CFLAGS} $O ../../lib/$(MACHTYPE)/jkweb.a
	mv ${AOUT} ${BINDIR}/stToXao
