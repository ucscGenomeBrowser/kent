include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = faSomeRecords.o

faSomeRecords: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/faSomeRecords $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/faSomeRecords${EXE}
