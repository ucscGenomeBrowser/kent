.c.o:
	${CC} ${COPT} ${CFLAGS} -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../inc -I../../inc -I../../../inc -c $*.c

L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a -lpthread

O = threadExp.o cacheQueue.o

threadExp: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/threadExp $O $(MYLIBS) $L

clean::
	rm -f ${O}
