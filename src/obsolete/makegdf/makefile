include ../../inc/common.mk


O = makegdf.o 

makegdf: $(O)
	${CC} ${COPT} ${CFLAGS} $O ../../lib/$(MACHTYPE)/jkweb.a
	mv ${AOUT} $(HOME)/bin/$(MACHTYPE)/makegdf
