kentSrc = ../..
include ../../inc/common.mk

MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = paraNodeStart.o

paraNodeStart: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ../bin/paraNodeStart $O ../lib/$(MACHTYPE)/paralib.a ../../lib/$(MACHTYPE)/jkweb.a ${L}
	${STRIP} ../bin/paraNodeStart${EXE}

clean::
	rm -f ${O}
