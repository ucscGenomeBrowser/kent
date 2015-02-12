include ../../inc/common.mk


O = chkGlue.o 

chkGlue.exe: $(O)
	${CC} ${COPT} ${CFLAGS} $O ../../lib/$(MACHTYPE)/jkweb.a ${L}
	mv ${AOUT} chkGlue.exe
	chmod a+rx chkGlue.exe
	mv chkGlue.exe ../../cgi-bin

clean::
	rm -f ${O}
