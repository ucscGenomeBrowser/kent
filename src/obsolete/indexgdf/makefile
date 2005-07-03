include ../../inc/common.mk


O = indexgdf.o 

indexgdf: $(O)
	${CC} ${COPT} ${CFLAGS} $O ../../lib/$(MACHTYPE)/jkweb.a
	mv ${AOUT} $(HOME)/bin/$(MACHTYPE)/indexgdf
