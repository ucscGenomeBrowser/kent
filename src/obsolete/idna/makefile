include ../../inc/makefile.h


O = idna.o 

idna.cgi: $(O)
	gcc $O ../lib/$(MACHTYPE)/jkweb.a
	mv a.out idna.cgi
	chmod a+rx idna.cgi
	mv idna.cgi ../cgi-bin
