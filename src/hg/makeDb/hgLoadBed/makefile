include ../../../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgLoadBed.o

hgLoadBed: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/hgLoadBed $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/hgLoadBed${EXE}
