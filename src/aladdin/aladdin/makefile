.c.o:
	gcc -I../../inc -c $*.c 

O = aladdin.o 

aladdin.exe: $(O)
	gcc $O ../../lib/$(MACHTYPE)/jkweb.a -lm
	mv a.out aladdin.exe
	chmod a+rx aladdin.exe
	mv aladdin.exe ../../cgi-bin

