include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = tfbsConsSort.o

tfbsConsSort: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/tfbsConsSort $O $(MYLIBS) $L
#	${STRIP} $(HOME)/bin/$(MACHTYPE)/tfbsConsSort${EXE}
