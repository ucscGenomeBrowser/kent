include ../../inc/common.mk


O =  refineAli.o

$(HOME)/bin/$(MACHTYPE)/refiAli: $(O)  
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/refiAli $O ../../lib/$(MACHTYPE)/jkOwnLib.a ../../lib/$(MACHTYPE)/jkweb.a -lm


