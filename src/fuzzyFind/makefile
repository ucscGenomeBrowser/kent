.c.o:
	gcc -ggdb -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../inc -c $*.c

O = ffapp.o 

fuzzyFind.exe: $(O) lib
	gcc $O ../lib/$(MACHTYPE)/jkOwnLib.a ../lib/$(MACHTYPE)/jkweb.a -lm
	mv a.out fuzzyFind.exe
	chmod a+rx fuzzyFind.exe
	mv fuzzyFind.exe ../cgi-bin

lib:
	cd ../lib; make; cd ../fuzzyFind
