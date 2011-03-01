include ../inc/common.mk

O = cbAli.o 

cbAli.exe: $(O)
	${CC} ${COPT} ${CFLAGS} $O ../lib/$(MACHTYPE)/jkweb.a ${L} -lm
	mv ${AOUT} cbAli.exe
	chmod a+rx cbAli.exe
	mv cbAli.exe ../cgi-bin

clean::
	rm -f ${O} cbAli.exe
