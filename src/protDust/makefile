.c.o:
	gcc -ggdb -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../inc -c $*.c

O = protDust.o 

protDust.exe: $(O)
	gcc $O ../lib/$(MACHTYPE)/jkweb.a
	mv a.out protDust.exe
	chmod a+rx protDust.exe
	mv protDust.exe ../cgi-bin
