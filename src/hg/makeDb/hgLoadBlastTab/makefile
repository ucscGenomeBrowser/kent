include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgLoadBlastTab.o

hgLoadBlastTab: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/hgLoadBlastTab $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/hgLoadBlastTab${EXE}
