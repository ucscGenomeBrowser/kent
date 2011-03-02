include ../../inc/common.mk

O = xenWorm.o 

xenWorm: $(O)
	${CC} ${COPT} ${CFLAGS} $O ../../lib/$(MACHTYPE)/jkOwnLib.a ../../lib/$(MACHTYPE)/jkweb.a ${L} -lm
	chmod a+x ${AOUT}
	chmod g-w ${AOUT}
	mv ${AOUT} ../../cgi-bin/xenWorm.exe

clean::
	rm -f ${O}
