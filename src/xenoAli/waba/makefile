include ../../inc/common.mk


O = waba.o wabaCrude.o

waba: $(O)
	${CC} ${COPT} ${CFLAGS} $O ../../lib/$(MACHTYPE)/jkOwnLib.a ../../lib/$(MACHTYPE)/jkweb.a -lm
	mv a.out $(HOME)/bin/$(MACHTYPE)/waba
