include ../../inc/common.mk


O = hitreport.o 

hitreport: $(O)
	gcc $O ../../lib/$(MACHTYPE)/jkweb.a
	mv a.out hitreport.exe
	chmod a+rx hitreport.exe
	mv hitreport.exe ../../cgi-bin
