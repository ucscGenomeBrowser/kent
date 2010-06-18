include ../../inc/common.mk


O =  cntGlue.o

${BINDIR}/cntGlue: $(O)  
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/cntGlue $O ../../lib/$(MACHTYPE)/jkweb.a

clean::
	rm -f ${O}
