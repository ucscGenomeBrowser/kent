include ../../inc/common.mk

O =  introns.o

${BINDIR}/introns: ${O}  
	${CC} -o ${BINDIR}/introns $O ../lib/cdnaAli.o \
		../../lib/${MACHTYPE}/jkweb.a
	${STRIP} ${BINDIR}/introns${EXE}

clean:
	rm -f introns.o

