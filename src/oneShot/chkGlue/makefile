.c.o:
	gcc -O -I../../inc -c $*.c

O = chkGlue.o 

chkGlue.exe: $(O)
	gcc $O ../../lib/$(MACHTYPE)/jkweb.a
	mv a.out chkGlue.exe
	chmod a+rx chkGlue.exe
	mv chkGlue.exe ../../cgi-bin
