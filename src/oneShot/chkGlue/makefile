include ../../inc/makefile.h


O = chkGlue.o 

chkGlue.exe: $(O)
	gcc $O ../../lib/$(MACHTYPE)/jkweb.a
	mv a.out chkGlue.exe
	chmod a+rx chkGlue.exe
	mv chkGlue.exe ../../cgi-bin
