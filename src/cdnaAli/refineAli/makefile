include ../../inc/common.mk


O =  refineAli.o

${BINDIR}/refiAli: $(O)  
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/refiAli $O ../../lib/$(MACHTYPE)/jkOwnLib.a ../../lib/$(MACHTYPE)/jkweb.a -lm

clean::
	rm -f ${O}
