include ../../inc/common.mk

O =  introns.o

${BINDIR}/introns: ${O}  
	${CC} -o ${BINDIR}/introns $O ../lib/cdnaAli.o \
		../../lib/${MACHTYPE}/jkweb.a ${L}
	${STRIP} ${BINDIR}/introns${EXE}

clean:
	rm -f introns.o

