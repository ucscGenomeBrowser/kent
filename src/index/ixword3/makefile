include ../../inc/common.mk


O = ixword3.o 

ixword3: $(O)
	${CC} ${COPT} ${CFLAGS} $O ../../lib/$(MACHTYPE)/jkweb.a $L
	mv ${AOUT} ${BINDIR}/ixword3

clean::
	rm -f ${O}
