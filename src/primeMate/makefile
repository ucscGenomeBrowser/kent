.c.o:
	gcc -I../inc -c $*.c

O = primeMate.o

primeMate.exe: $(O) 
	gcc $O ../lib/$(MACHTYPE)/jkweb.a
	mv a.out primeMate.exe
	chmod a+rx primeMate.exe
	chmod g-w primeMate.exe
	mv primeMate.exe $(HOME)/.html/cgi-bin
