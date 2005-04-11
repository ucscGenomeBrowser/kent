include ../../../inc/common.mk

O = cCoder.o main.o pfBindVars.o pfCheck.o pfParse.o pfScope.o pfToken.o pfType.o

paraFlow: $(O)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/paraFlow $O ../../../lib/$(MACHTYPE)/jkweb.a -lm

test:
	paraFlow test.pf
	gcc -c out.c 
	gcc -O -o p.out out.o ../runtime/runtime.a ../../../lib/powerpc/jkweb.a -lm
	p.out
