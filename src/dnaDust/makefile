.c.o:
	gcc -I../inc -c $*.c

O = dnadust.o 

dnadust.exe: $(O)
	gcc $O ../lib/$(MACHTYPE)/jkweb.a
	mv a.out dnadust.exe
	chmod a+rx dnadust.exe
	mv dnadust.exe ../cgi-bin
