include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = pslToXa.o

pslToXa: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/pslToXa $O $(MYLIBS) $L
#	${STRIP} $(HOME)/bin/$(MACHTYPE)/pslToXa${EXE}
