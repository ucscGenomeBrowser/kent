.c.o:
	gcc -O -I../inc -c $*.c

O = idbQuery.o 

idbQuery.exe: $(O)
	gcc $O ../lib/$(MACHTYPE)/jkweb.a
	mv a.out idbQuery.exe
	chmod a+rx idbQuery.exe
	mv idbQuery.exe ../cgi-bin
