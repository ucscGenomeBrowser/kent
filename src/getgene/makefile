include ../inc/common.mk

O = getgene.o 

getgene.exe: $(O)
	${CC} ${COPT} ${CFLAGS} $O ../lib/$(MACHTYPE)/jkweb.a
	mv a.out getgene.exe
	chmod a+rx getgene.exe
	chmod g-w getgene.exe

push:	
	mv getgene.exe $(HOME)/.html/cgi-bin
