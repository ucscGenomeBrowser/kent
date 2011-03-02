include ../../inc/common.mk


O = dynAlign.o 

dynAlign.exe: $(O)
	${CC} ${COPT} ${CFLAGS} $O ../../lib/$(MACHTYPE)/jkweb.a ${L} -lm
	mv ${AOUT} dynAlign.exe
	chmod a+rx dynAlign.exe
	mv dynAlign.exe ../../cgi-bin

clean::
	rm -f ${O}
