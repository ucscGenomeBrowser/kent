include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = faPolyASizes.o

faPolyASizes: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/faPolyASizes${EXE} $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/faPolyASizes${EXE}
