include ../../inc/common.mk


L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = gpToGtf.o

gpToGtf: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/gpToGtf $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/gpToGtf${EXE}
