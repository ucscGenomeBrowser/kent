.c.o:
	gcc -I../../inc -c $*.c 

O = xaShow.o 

xaShow.exe: $(O)
	gcc $O ../../lib/$(MACHTYPE)/jkweb.a -lm
	mv a.out xaShow.exe
	chmod a+rx xaShow.exe
	chmod g-w xaShow.exe
	mv xaShow.exe $(HOME)/.html/cgi-bin

