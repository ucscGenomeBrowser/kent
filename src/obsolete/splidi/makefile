include ../../inc/common.mk


O =  splidi.o

splidi.exe: $(O)  lib
	${CC} ${COPT} ${CFLAGS} $O ../lib/$(MACHTYPE)/jkweb.a
	mv a.out splidi.exe
	chmod a+rx splidi.exe
	mv splidi.exe ../cgi-bin

lib: 
	cd ../lib;make; cd ../splidi

