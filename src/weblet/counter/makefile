.c.o:
	gcc -I../../inc -c $*.c

O = counter.o 

counter: $(O)
	gcc $O ../../lib/$(MACHTYPE)/jkweb.a
	mv a.out counter.exe
	chmod a+rx counter.exe
	strip counter.exe
	mv counter.exe ../../cgi-bin
