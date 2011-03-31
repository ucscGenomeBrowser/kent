include ../../../inc/common.mk

O = asmCoder.o backEnd.o builtIn.o cCaseCode.o \
    cCoder.o checkPara.o checkPoly.o cMain.o cPara.o \
    codedType.o constFold.o ctar.o defFile.o gnuMac.o \
    isx.o isxLiveVar.o isxToPentium.o \
    main.o optBranch.o parseDump.o parseInto.o pentCast.o \
    pentCode.o pentConst.o \
    pentStruct.o pfBindVars.o pfParse.o pfScope.o pfToken.o pfType.o \
    recodedType.o refCount.o tokInto.o util.o

paraFlow: $(O)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/paraFlow $O ../../../lib/$(MACHTYPE)/jkweb.a ${L} -lm


test:
	cd ../testProg && ${MAKE}

q:
	cd ../testProg && ${MAKE} q

ret:
	gcc -c t.c 
	gcc -c out.c
	gcc -O -o t out.o t.o ../runtime/runtime.a ../../../lib/powerpc/jkweb.a -lm
	t

clean::
	rm -f ${O}
