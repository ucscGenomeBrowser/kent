include ../../inc/common.mk


L = -lm $(MYSQLLIBS)
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = gpToGtf.o

gpToGtf: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/gpToGtf $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/gpToGtf
