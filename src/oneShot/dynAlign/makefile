include ../../inc/common.mk


O = dynAlign.o 

dynAlign.exe: $(O)
	${CC} ${COPT} ${CFLAGS} $O ../../lib/$(MACHTYPE)/jkweb.a -lm
	mv a.out dynAlign.exe
	chmod a+rx dynAlign.exe
	mv dynAlign.exe ../../cgi-bin


