include ../../../inc/common.mk

O = builtin.o cCoder.o main.o pfBindVars.o pfCheck.o \
    pfParse.o pfScope.o pfToken.o pfType.o util.o

paraFlow: $(O)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/paraFlow $O ../../../lib/$(MACHTYPE)/jkweb.a -lm

t:
	paraFlow test.pf

retest:
	gcc -c test.c 
	gcc -O -o test test.o ../runtime/runtime.a ../../../lib/powerpc/jkweb.a -lm
	test
