include ../../inc/common.mk


O =  sortFilt.o 

${BINDIR}/sortFilt: $(O)  
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/sortFilt $O ../../lib/$(MACHTYPE)/jkweb.a

clean::
	rm -f ${O}
