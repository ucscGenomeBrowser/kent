include ../../inc/makefile.h

O = xenWorm.o 

xenWorm: $(O)
	gcc $O ../../lib/$(MACHTYPE)/jkOwnLib.a ../../lib/$(MACHTYPE)/jkweb.a -lm
	chmod a+x a.out
	chmod g-w a.out
	mv a.out ../../cgi-bin/xenWorm.exe
