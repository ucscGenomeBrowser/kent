.c.o:
	gcc -I../../inc -c $*.c 

O = dynAlign.o 

dynAlign.exe: $(O)
	gcc $O ../../lib/$(MACHTYPE)/jkweb.a -lm
	mv a.out dynAlign.exe
	chmod a+rx dynAlign.exe
	mv dynAlign.exe ../../cgi-bin


