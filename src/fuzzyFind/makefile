include ../inc/common.mk

O = ffapp.o 

fuzzyFind.exe: $(O) lib
	${CC} ${COPT} ${CFLAGS} $O ../lib/$(MACHTYPE)/jkOwnLib.a ../lib/$(MACHTYPE)/jkweb.a ${L} -lm
	mv ${AOUT} fuzzyFind.exe
	chmod a+rx fuzzyFind.exe

push:	
	mv fuzzyFind.exe ../cgi-bin

lib:
	cd ../lib && ${MAKE}

clean::
	rm -f ${O}
