.c.o:
	gcc -O -ggdb -I../inc -I../../inc -c $*.c

O = paraNode.o

paraNode: $(O)
	gcc -o paraNode $O ../lib/$(MACHTYPE)/paralib.a ../../lib/$(MACHTYPE)/jkweb.a

