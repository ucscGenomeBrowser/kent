include ../../inc/common.mk


O = dynAlign.o 

dynAlign: $(O)
	${CC} ${COPT} ${CFLAGS} $O ../../lib/$(MACHTYPE)/jkOwnLib.a ../../lib/$(MACHTYPE)/jkweb.a -lm
	chmod a+x ${AOUT}
	chmod g-w ${AOUT}
	
push:	
	mv ${AOUT} ${CGI_BIN}-${USER}/dynAlign.exe

clean::
	rm -f "{O}" "${AOUT}"
