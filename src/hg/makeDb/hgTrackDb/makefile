include ../../../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgTrackDb.o


hgTrackDb: $O 
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/hgTrackDb $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/hgTrackDb${EXE}

