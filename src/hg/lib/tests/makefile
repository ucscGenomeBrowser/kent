XINC = -I $(MYSQLINC)
include ../../../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a 

O = testHdb.o

testHdb: $O $(MYLIBS)
	gcc -o testHdb $(O) $(MYLIBS) $L

clean:
	rm -f $(O) testHdb

