include ../../inc/common.mk


O = findCdna.o 

findCdna: $(O)
	${CC} ${COPT} ${CFLAGS} $O ../../lib/$(MACHTYPE)/jkweb.a
	mv ${AOUT} ${BINDIR}/findCdna
