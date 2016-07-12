kentSrc = ../..
include ../../inc/common.mk


L += $(MYSQLLIBS) -lm 

O = killRabbit.o

killRabbit: lib $O ../../lib/$(MACHTYPE)/jkweb.a
	${CC} ${COPT} ${CFLAGS} $O ../../lib/$(MACHTYPE)/jkweb.a $L
	mv ${AOUT} killRabbit
	${STRIP} killRabbit${EXE}

lib:
	cd ../../lib && ${MAKE}

clean::
	rm -f ${O}
