include ../../inc/makefile.h


O = checkLayout.o 

checkLayout: $(O)
	gcc -o ../../cgi-bin/checkLayout $O ../../lib/$(MACHTYPE)/jkweb.a
	strip ../../cgi-bin/checkLayout
