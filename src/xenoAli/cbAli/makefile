include ../../inc/common.mk


O = cbAli.o 

cbAli: $(O)
	${CC} ${COPT} ${CFLAGS} $O ../../lib/$(MACHTYPE)/jkOwnLib.a ../../lib/$(MACHTYPE)/jkweb.a ${L}
	mv ${AOUT} ${BINDIR}/cbAli

clean::
	rm -f ${O}
