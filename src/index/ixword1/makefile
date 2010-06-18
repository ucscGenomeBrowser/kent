include ../../inc/common.mk 


O = ixword1.o 

ixword1: $(O)
	${CC} ${COPT} ${CFLAGS} $O ../../lib/$(MACHTYPE)/jkweb.a $L
	mv ${AOUT} ${BINDIR}/ixword1

clean::
	rm -f ${O}
