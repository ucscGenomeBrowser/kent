include ../../inc/common.mk


O =  sortFilt.o 

${BINDIR}/sortFilt: $(O)  
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/sortFilt $O ../../lib/$(MACHTYPE)/jkweb.a ${L}

clean::
	rm -f ${O}
