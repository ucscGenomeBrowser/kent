include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = $(HOME)/kent/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a 
CFLAGS= -g

O = kgName.o

kgName: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/kgName $O $(MYLIBS) $L
#	strip $(HOME)/bin/$(MACHTYPE)/kgName
