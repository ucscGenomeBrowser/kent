include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = $(HOME)/kent/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a 
CFLAGS= -g


all : kgName fbName 

kgName: $(MYLIBS) kgName.o
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/kgName kgName.o $(MYLIBS) $L
fbName: $(MYLIBS) fbName.o
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/fbName fbName.o $(MYLIBS) $L
#	strip $(HOME)/bin/$(MACHTYPE)/kgName
