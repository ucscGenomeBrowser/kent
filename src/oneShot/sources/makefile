include ../../inc/common.mk


O = sources.o 

$(HOME)/bin/$(MACHTYPE)/sources: $(O)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/sources $O ../../lib/$(MACHTYPE)/jkweb.a
