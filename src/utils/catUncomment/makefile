include ../../inc/common.mk


O = catUncomment.o 

catUncomment: $(O)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/catUncomment $O ../../lib/$(MACHTYPE)/jkweb.a
	${STRIP} $(HOME)/bin/$(MACHTYPE)/catUncomment${EXE}


