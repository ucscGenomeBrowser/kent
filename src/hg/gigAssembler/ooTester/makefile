include ../../../inc/common.mk


L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = ooTester.o 

ooTester: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/ooTester $O $(MYLIBS) $L


lib:
	cd ../../lib && ${MAKE}
	cd ../../../lib && ${MAKE}
