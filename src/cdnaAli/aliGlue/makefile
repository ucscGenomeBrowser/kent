include ../../inc/common.mk

O =  aliGlue.o

${BINDIR}/aliGlue: $(O)  
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/aliGlue $O ../../lib/$(MACHTYPE)/jkweb.a ../../lib/$(MACHTYPE)/jkOwnLib.a -lm ${L}

strip:
	${STRIP} ${BINDIR}/aliGlue${EXE}

clean::
	rm -f ${O}
