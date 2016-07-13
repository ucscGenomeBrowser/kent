kentSrc = ../..
include ../../inc/common.mk


L += $(MYSQLLIBS) -lm

O = newSang6.o

newSang6: $O ../../lib/$(MACHTYPE)/jkweb.a
	${CC} ${COPT} ${CFLAGS} -o newSang6 $O ../../lib/$(MACHTYPE)/jkweb.a $L

lib:
	cd ../../lib && ${MAKE}

clean::
	rm -f ${O}
