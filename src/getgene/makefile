.c.o:
	gcc -I../inc -c $*.c

O = getgene.o 

getgene.exe: $(O)
	gcc $O ../lib/$(MACHTYPE)/jkweb.a
	mv a.out getgene.exe
	chmod a+rx getgene.exe
	mv getgene.exe ../cgi-bin
