.c.o:
	gcc -I../inc -c $*.c

O = tracks.o 

tracks.exe: $(O)
	gcc $O ../lib/$(MACHTYPE)/jkweb.a
	mv a.out tracks.exe
	chmod g-w tracks.exe
	chmod a+rx tracks.exe
	mv tracks.exe $(HOME)/.html/cgi-bin
