.c.o:
	gcc -ggdb -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../inc -c $*.c

O = wormAli.o 

wormAli.exe: $(O)
	gcc $O ../lib/$(MACHTYPE)/jkOwnLib.a ../lib/$(MACHTYPE)/jkweb.a -lm
	mv a.out wormAli.exe
	chmod g-w wormAli.exe
	chmod a+rx wormAli.exe
	mv wormAli.exe $(HOME)/.html/cgi-bin
