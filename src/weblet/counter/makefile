include ../../inc/common.mk


O = counter.o 
L += -lz

counter: $(O)
	${CC} ${COPT} ${CFLAGS} $O ../../lib/$(MACHTYPE)/jkweb.a ${L}
	mv ${AOUT} counter${EXE}
	chmod a+rx counter${EXE}
	${STRIP} counter${EXE}

push:
	mv counter.exe ${CGI_BIN}-${USER}

clean::
	rm -f ${O} ${AOUT} counter${EXE}
