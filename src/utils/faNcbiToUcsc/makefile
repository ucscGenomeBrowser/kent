include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = faNcbiToUcsc.o

faNcbiToUcsc: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/faNcbiToUcsc $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/faNcbiToUcsc${EXE}
