include ../inc/makefile.h

O = getgene.o 

getgene.exe: $(O)
	gcc $O ../lib/$(MACHTYPE)/jkweb.a
	mv a.out getgene.exe
	chmod a+rx getgene.exe
	chmod g-w getgene.exe
	mv getgene.exe $(HOME)/.html/cgi-bin
