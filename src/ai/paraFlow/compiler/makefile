include ../../../inc/common.mk

O = main.o pfToken.o pfScope.o

paraFlow: $(O)
	${CC} ${COPT} ${CFLAGS} -o paraFlow $O ../../../lib/$(MACHTYPE)/jkweb.a -lm


P = test.o pfToken.o pfScope.o

test: $P
	${CC} ${COPT} ${CFLAGS} -o test $P ../../../lib/$(MACHTYPE)/jkweb.a -lm

