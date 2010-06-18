include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a 

O = trfBig.o

trfBig: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/trfBig $O $(MYLIBS) $L

clean::
	rm -f ${O}
