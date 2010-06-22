include ../../inc/common.mk


O =  patSpace.o

${BINDIR}/patSpace: $(O)  
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/patSpace $O ../../lib/$(MACHTYPE)/jkweb.a

clean::
	rm -f ${O}
