include ../../../inc/common.mk


L = $(MYSQLLIBS) -lm 
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a


O = checkDbSync.o

checkDbSync: $(O)
	gcc -o $(HOME)/bin/$(MACHTYPE)/checkDbSync $O $(MYLIBS) $(L)
	${STRIP} $(HOME)/bin/$(MACHTYPE)/checkDbSync${EXE}

