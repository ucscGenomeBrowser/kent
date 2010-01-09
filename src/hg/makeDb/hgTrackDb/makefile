include ../../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgTrackDb.o


hgTrackDb: $O 
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/hgTrackDb $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/hgTrackDb${EXE}

clean:
	rm -f $O

