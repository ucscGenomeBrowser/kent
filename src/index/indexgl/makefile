.c.o:
	gcc -I../../inc -c $*.c

O = indexgl.o 

indexgl: $(O) 
	gcc $O ../../lib/$(MACHTYPE)/jkweb.a
	mv a.out $(HOME)/bin/$(MACHTYPE)/indexgl

lib:
	cd ../../lib;make; cd ../index/indexgl

