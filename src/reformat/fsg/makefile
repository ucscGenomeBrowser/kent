include ../../inc/common.mk


O = fsg.o 

$(HOME)/bin/$(MACHTYPE)/fsg: $(O)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/fsg $O ../../lib/$(MACHTYPE)/jkweb.a
