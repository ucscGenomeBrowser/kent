.c.o:
	gcc -O -I../../inc -c $*.c

O = dynAlign.o 

dynAlign: $(O)
	gcc $O ../../lib/$(MACHTYPE)/jkweb.a -lm
	chmod a+x a.out
	chmod g-w a.out
	mv a.out ../../cgi-bin/dynAlign.exe
