include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = faSomeRecords.o

faSomeRecords: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/faSomeRecords $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/faSomeRecords${EXE}
