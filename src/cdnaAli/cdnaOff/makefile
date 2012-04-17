include ../../inc/common.mk

O =  cdnaOff.o 

cdnaOff: $(O)  
	cd ../lib && ${MAKE}
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/cdnaOff $O ../lib/cdnaAli.o ../../lib/$(MACHTYPE)/jkweb.a ${L}

clean::
	rm -f ${O}
