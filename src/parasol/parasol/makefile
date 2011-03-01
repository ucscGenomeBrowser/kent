include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = parasol.o 

parasol: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ../bin/parasol $O ../lib/$(MACHTYPE)/paralib.a ../../lib/$(MACHTYPE)/jkweb.a ${L}
	${STRIP} ../bin/parasol${EXE}

clean::
	rm -f ${O}
