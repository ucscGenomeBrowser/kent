.c.o:
	gcc -I../inc -c $*.c

O =  splidi.o

splidi.exe: $(O)  lib
	gcc $O ../lib/$(MACHTYPE)/jkweb.a
	mv a.out splidi.exe
	chmod a+rx splidi.exe
	mv splidi.exe ../cgi-bin

lib: 
	cd ../lib;make; cd ../splidi

