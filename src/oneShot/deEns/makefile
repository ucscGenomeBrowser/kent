.c.o:
	gcc -O -ggdb -I../../inc -c $*.c

O = deEns.o 

deEns: $(O)
	gcc -o deEns $O ../../lib/$(MACHTYPE)/jkweb.a


lib:
	cd ../../lib; make
