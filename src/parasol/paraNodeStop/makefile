include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = paraNodeStop.o

paraNodeStop: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ../bin/paraNodeStop $O ../lib/$(MACHTYPE)/paralib.a ../../lib/$(MACHTYPE)/jkweb.a ${L}
	${STRIP} ../bin/paraNodeStop${EXE}
	chmod 744 ../bin/paraNodeStop

clean::
	rm -f ${O}
