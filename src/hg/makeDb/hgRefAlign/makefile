.c.o:
	gcc ${OPT} -Wimplicit -I../../inc -I../../../inc -c $*.c

OPT = -ggdb -O

L = $(MYSQLLIBS) -lm
PROG =  $(HOME)/bin/$(MACHTYPE)/hgRefAlign
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgRefAlign.o hgRefAlignWebb.o hgRefAlignChromList.o

${PROG}: $O ../../../lib/$(MACHTYPE)/jkweb.a
	gcc ${OPT} -o ${PROG} $O $(MYLIBS) $L

clean:
	rm -f ${O}

lib:
	cd ../../../lib; make

hgRefAlignWebb.o: hgRefAlignWebb.h
hgRefAlignChromList.o: hgRefAlignChromList.h
hgRefAlign.o: hgRefAlignWebb.h hgRefAlignChromList.h
