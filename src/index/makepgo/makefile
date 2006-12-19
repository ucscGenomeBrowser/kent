include ../../inc/common.mk


O = makepgo.o 

makepgo: $(O) 
	${CC} ${COPT} ${CFLAGS} $O ../../lib/$(MACHTYPE)/jkweb.a
	mv ${AOUT} ${BINDIR}/makepgo

lib:
	cd ../../lib && ${MAKE}

