include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = faRc.o

faRc: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/faRc $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/faRc${EXE}
