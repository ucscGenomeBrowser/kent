include ../inc/common.mk

O = wormAli.o 

wormAli.exe: $(O)
	${CC} ${COPT} ${CFLAGS} $O ../lib/$(MACHTYPE)/jkOwnLib.a ../lib/$(MACHTYPE)/jkweb.a ${L} -lm
	mv ${AOUT} wormAli.exe

install: wormAli.exe	
	chmod g-w wormAli.exe
	chmod a+rx wormAli.exe
	mv wormAli.exe ${CGI_BIN}/

clean::
	rm -f wormAli.exe ${AOUT} ${O}
