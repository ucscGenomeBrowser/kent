include ../../inc/common.mk


O = ixxenost.o 

ixxenost: $(O)
	${CC} ${COPT} ${CFLAGS} $O ../../lib/$(MACHTYPE)/jkweb.a
	mv a.out $(HOME)/bin/$(MACHTYPE)/ixxenost
