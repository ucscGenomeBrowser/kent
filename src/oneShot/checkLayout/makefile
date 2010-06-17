include ../../inc/common.mk


O = checkLayout.o 

checkLayout: $(O)
	${CC} ${COPT} ${CFLAGS} -o ../../cgi-bin/checkLayout $O ../../lib/$(MACHTYPE)/jkweb.a
	${STRIP} ../../cgi-bin/checkLayout${EXE}

clean::
	rm -f ${O}
