include ../../../inc/common.mk

O = main.o pfBindVars.o pfParse.o pfScope.o pfToken.o pfType.o

paraFlow: $(O)
	${CC} ${COPT} ${CFLAGS} -o paraFlow $O ../../../lib/$(MACHTYPE)/jkweb.a -lm


