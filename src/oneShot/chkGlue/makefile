include ../../inc/common.mk


O = chkGlue.o 

chkGlue.exe: $(O)
	${CC} ${COPT} ${CFLAGS} $O ../../lib/$(MACHTYPE)/jkweb.a
	mv a.out chkGlue.exe
	chmod a+rx chkGlue.exe
	mv chkGlue.exe ../../cgi-bin
