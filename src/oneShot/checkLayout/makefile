include ../../inc/common.mk


O = checkLayout.o 

checkLayout: $(O)
	gcc -o ../../cgi-bin/checkLayout $O ../../lib/$(MACHTYPE)/jkweb.a
	${STRIP} ../../cgi-bin/checkLayout${EXE}
