include ../../inc/common.mk


O = makepgo.o 

makepgo: $(O) 
	${CC} ${COPT} ${CFLAGS} $O ../../lib/$(MACHTYPE)/jkweb.a $L
	mv ${AOUT} ${BINDIR}/makepgo

lib:
	cd ../../lib && ${MAKE}

clean::
	rm -f ${O}
