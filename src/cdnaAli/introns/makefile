include ../../inc/common.mk

O =  introns.o

${HOME}/bin/${MACHTYPE}/introns: ${O}  
	${CC} -o ${HOME}/bin/${MACHTYPE}/introns $O ../lib/cdnaAli.o \
		../../lib/${MACHTYPE}/jkweb.a
	strip ${HOME}/bin/${MACHTYPE}/introns

clean:
	rm -f introns.o

