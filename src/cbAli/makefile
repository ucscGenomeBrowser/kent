include ../inc/common.mk

O = cbAli.o 

cbAli.exe: $(O)
	${CC} ${COPT} ${CFLAGS} $O ../lib/$(MACHTYPE)/jkweb.a -lm
	mv a.out cbAli.exe
	chmod a+rx cbAli.exe
	mv cbAli.exe ../cgi-bin


