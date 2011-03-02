include ../inc/common.mk

O = getgene.o 

getgene.exe: $(O)
	${CC} ${COPT} ${CFLAGS} $O ../lib/$(MACHTYPE)/jkweb.a ${L}
	mv ${AOUT} getgene.exe

install: getgene.exe	
	chmod a+rx getgene.exe
	chmod g-w getgene.exe
	mv getgene.exe ${CGI_BIN}/

clean::
	rm -f getgene.exe ${O} ${AOUT}
