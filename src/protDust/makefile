include ../inc/common.mk

O = protDust.o 

protDust.exe: $(O)
	${CC} ${COPT} ${CFLAGS} $O ../lib/$(MACHTYPE)/jkweb.a
	mv a.out protDust.exe
	chmod a+rx protDust.exe

push:	
	mv protDust.exe ${CGI_BIN}-${USER}
