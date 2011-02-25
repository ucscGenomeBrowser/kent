include ../../inc/common.mk


O = hitreport.o 

hitreport: $(O)
	${CC} ${COPT} ${CFLAGS} $O ../../lib/$(MACHTYPE)/jkweb.a ${L}
	mv ${AOUT} hitreport.exe
	chmod a+rx hitreport.exe

push:	
	mv hitreport.exe ${CGI_BIN}-${USER}

clean::
	rm -f ${O} ${AOUT} hitreport.exe
