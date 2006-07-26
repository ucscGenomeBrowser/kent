include ../../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgFlyBase.o

hgFlyBase: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/hgFlyBase $O $(MYLIBS) $L
#	${STRIP} $(HOME)/bin/$(MACHTYPE)/hgFlyBase${EXE}
