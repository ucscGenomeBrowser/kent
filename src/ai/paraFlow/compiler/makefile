include ../../../inc/common.mk

O = main.o pfParse.o pfScope.o pfToken.o 

paraFlow: $(O)
	${CC} ${COPT} ${CFLAGS} -o paraFlow $O ../../../lib/$(MACHTYPE)/jkweb.a -lm


