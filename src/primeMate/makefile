include ../inc/common.mk

O = primeMate.o

primeMate.exe: $(O) 
	${CC} ${COPT} ${CFLAGS} $O ../lib/$(MACHTYPE)/jkweb.a ${L}
	mv ${AOUT} primeMate.exe

install: primeMate.exe	
	chmod a+rx primeMate.exe
	chmod g-w primeMate.exe
	mv primeMate.exe ${CGI_BIN}/

clean::
	rm -f primeMate.exe ${O} ${AOUT}
