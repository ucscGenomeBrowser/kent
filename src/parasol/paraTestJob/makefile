include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = paraTestJob.o

paraTestJob: $O $(MYLIBS)
	gcc -ggdb -o ../bin/paraTestJob $O $(MYLIBS) $L
	strip ../bin/paraTestJob
