include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = para.o jobDb.o jobResult.o

../bin/para: $O $(MYLIBS)
	@${MKDIR} ../bin
	gcc -o ../bin/para $O ../lib/$(MACHTYPE)/paralib.a ../../lib/$(MACHTYPE)/jkweb.a
	strip ../bin/para
