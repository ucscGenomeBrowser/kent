include ../inc/common.mk

O = primeMate.o

primeMate.exe: $(O) 
	${CC} ${COPT} ${CFLAGS} $O ../lib/$(MACHTYPE)/jkweb.a
	mv a.out primeMate.exe
	chmod a+rx primeMate.exe
	chmod g-w primeMate.exe

push:	
	mv primeMate.exe $(HOME)/.html/cgi-bin
