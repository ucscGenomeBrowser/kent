include ../../../../inc/common.mk

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = ooGreedy.o

ooGreedy.84:
	${CC} ${COPT} ${CFLAGS} -Wimplicit -I../../inc -I../../../inc -c ooGreedy.c
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/ooGreedy.84 ooGreedy.o $(MYLIBS) $L

lib:
	cd ../../lib; make
	cd ../../../lib; make
