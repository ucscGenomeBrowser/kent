include ../../inc/common.mk

O =  introns.o

${HOME}/bin/${MACHTYPE}/introns: ${O}  
	${CC} -o ${HOME}/bin/${MACHTYPE}/introns $O ../lib/cdnaAli.o \
		../../lib/${MACHTYPE}/jkweb.a
	${STRIP} ${HOME}/bin/${MACHTYPE}/introns${EXE}

clean:
	rm -f introns.o

