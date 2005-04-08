include ../../../inc/common.mk

O = main.o pfBindVars.o pfCheck.o pfParse.o pfScope.o pfToken.o pfType.o

paraFlow: $(O)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/paraFlow $O ../../../lib/$(MACHTYPE)/jkweb.a -lm


