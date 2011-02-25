include ../inc/common.mk

O = protDust.o 

protDust.exe: $(O)
	${CC} ${COPT} ${CFLAGS} $O ../lib/$(MACHTYPE)/jkweb.a ${L}
	mv ${AOUT} protDust.exe
	chmod a+rx protDust.exe

push:	
	mv protDust.exe ${CGI_BIN}-${USER}

clean::
	rm -f ${O} protDust.exe ${AOUT}
