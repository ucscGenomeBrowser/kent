include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = nibCheck.o

nibCheck: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/nibCheck $O $(MYLIBS) $L
	${CC} ${COPT} ${CFLAGS} $O $(MYLIBS) -lm
	mv a.out nibCheck
	chmod a+rx nibCheck
