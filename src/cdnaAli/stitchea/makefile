include ../../inc/common.mk


O =  stitchea.o

${BINDIR}/stitchea: $(O)  
	${CC} ${COPT} ${CFLAGS} -lm -o ${BINDIR}/stitchea $O ../../lib/$(MACHTYPE)/jkweb.a ${L}

clean::
	rm -f ${O}
