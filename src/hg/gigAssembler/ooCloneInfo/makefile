include ../../../inc/common.mk


L = $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = ooCloneInfo.o

ooCloneInfo: $O 
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/ooCloneInfo $O $(MYLIBS) $L

