include ../../inc/common.mk


O = indexfa.o 

indexfa: $(O) lib
	${CC} ${COPT} ${CFLAGS} -lm $O ../../lib/$(MACHTYPE)/jkweb.a $L
	mv ${AOUT} ${BINDIR}/indexfa

lib:
	cd ../../lib && ${MAKE}

clean::
	rm -f ${O}
