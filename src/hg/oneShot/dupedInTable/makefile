include ../../../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = dupedInTable.o

dupedInTable: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/dupedInTable $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/dupedInTable${EXE}
