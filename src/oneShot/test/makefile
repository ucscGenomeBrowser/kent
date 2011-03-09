include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = test.o

test: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o test $O $(MYLIBS) $L
#	${STRIP} test${EXE}

clean::
	rm -f ${O}
