.c.o:
	gcc -I../inc -c $*.c

O = idna.o 

idna.cgi: $(O)
	gcc $O ../lib/$(MACHTYPE)/jkweb.a
	mv a.out idna.cgi
	chmod a+rx idna.cgi
	mv idna.cgi ../cgi-bin
