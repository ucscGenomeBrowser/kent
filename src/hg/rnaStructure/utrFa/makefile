include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm 
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = utrFa.o

utrFa: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/utrFa $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/utrFa${EXE}
