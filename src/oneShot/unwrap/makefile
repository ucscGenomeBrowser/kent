.c.o:
	gcc -O -ggdb -I../../inc -c $*.c

O = unwrap.o 

unwrap: $(O)
	gcc -o unwrap $O ../../lib/$(MACHTYPE)/jkweb.a


