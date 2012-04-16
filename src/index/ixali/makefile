include ../../inc/common.mk


O = ixali.o 

ixali: $(O)
	${CC} ${COPT} ${CFLAGS} -lm $O ../../lib/$(MACHTYPE)/jkweb.a $L
	mv ${AOUT} ${BINDIR}/ixali

clean::
	rm -f ${O}
