include ../../inc/common.mk


O = counter.o 

counter: $(O)
	${CC} ${COPT} ${CFLAGS} $O ../../lib/$(MACHTYPE)/jkweb.a
	mv ${AOUT} counter.exe
	chmod a+rx counter.exe
	${STRIP} counter.exe${EXE}

push:
	mv counter.exe ${CGI_BIN}-${USER}
