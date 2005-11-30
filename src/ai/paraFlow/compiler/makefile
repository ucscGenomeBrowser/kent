include ../../../inc/common.mk

O = builtin.o cCoder.o codedType.o defFile.o \
    main.o pfBindVars.o pfCheck.o \
    pfParse.o pfScope.o pfToken.o pfType.o parseInto.o \
    recodedType.o tokInto.o util.o

paraFlow: $(O)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/paraFlow $O ../../../lib/$(MACHTYPE)/jkweb.a -lm


test:
	cd ../testProg/oneFile; make
	cd ../testProg/easy; make
	cd ../testProg/modules; make
	cd ../testProg/fixes; make

ret:
	gcc -c t.c 
	gcc -c out.c
	gcc -O -o t out.o t.o ../runtime/runtime.a ../../../lib/powerpc/jkweb.a -lm
	t
