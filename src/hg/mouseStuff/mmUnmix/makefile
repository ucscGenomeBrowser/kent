include ../../../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = mmUnmix.o

mmUnmix: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/mmUnmix $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/mmUnmix${EXE}
