.c.o:
	gcc -I../inc -c $*.c

O = scanIntrons.o

scanIntrons.exe: $(O) lib
	gcc $O ../lib/$(MACHTYPE)/jkweb.a
	mv a.out scanIntrons.exe
	chmod a+rx scanIntrons.exe
	mv scanIntrons.exe ../cgi-bin

lib: 
	cd ../lib;make; cd ../scanIntrons
