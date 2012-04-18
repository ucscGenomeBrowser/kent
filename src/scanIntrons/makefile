include ../inc/common.mk

O = scanIntrons.o

scanIntrons.exe: $(O) lib
	${CC} ${COPT} ${CFLAGS} $O ../lib/$(MACHTYPE)/jkweb.a ${L}
	mv ${AOUT} scanIntrons.exe
	chmod a+rx scanIntrons.exe

push:	
	mv scanIntrons.exe ${CGI_BIN}-${USER}

lib: 
	cd ../lib && ${MAKE}

clean::
	rm -f ${O} scanIntrons.exe ${AOUT}
