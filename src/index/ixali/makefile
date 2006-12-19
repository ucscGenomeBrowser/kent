include ../../inc/common.mk


O = ixali.o 

ixali: $(O)
	${CC} ${COPT} ${CFLAGS} $O ../../lib/$(MACHTYPE)/jkweb.a
	mv ${AOUT} ${BINDIR}/ixali
