include ../../../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = mousePoster.o

mousePoster: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/mousePoster $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/mousePoster${EXE}
