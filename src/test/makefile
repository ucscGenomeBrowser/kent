../cgi-bin/test.cgi: ../lib/$(MACHTYPE)/jkweb.a test.c
	gcc -I../inc test.c ../lib/$(MACHTYPE)/jkweb.a -lm
	chmod a+x a.out
	mv a.out ../cgi-bin/test.cgi
