.c.o:
	gcc -ggdb -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../inc -c $*.c

O = cbAli.o 

cbAli.exe: $(O)
	gcc $O ../lib/$(MACHTYPE)/jkweb.a -lm
	mv a.out cbAli.exe
	chmod a+rx cbAli.exe
	mv cbAli.exe ../cgi-bin


