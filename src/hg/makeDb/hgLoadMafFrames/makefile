include ../../../inc/common.mk
HG_WARN = ${HG_WARN_ERR}

MYLIBDIR = ../../../lib/$(MACHTYPE)
L = $(MYSQLLIBS) -lm
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgLoadMafFrames.o

hgLoadMafFrames: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/hgLoadMafFrames $O $(MYLIBS) $L


clean:
	rm -f  *.o
