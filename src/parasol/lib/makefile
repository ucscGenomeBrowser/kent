.c.o:
	gcc -ggdb -O -Wimplicit -I../inc -I../../inc -c $*.c


O = paraLib.o

$(MACHTYPE)/paralib.a: $(O) 
	ar rcu ../lib/$(MACHTYPE)/paralib.a $(O)
	ranlib ../lib/$(MACHTYPE)/paralib.a

