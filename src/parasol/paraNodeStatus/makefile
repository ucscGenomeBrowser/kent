include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = paraNodeStatus.o

paraNodeStatus: $O $(MYLIBS)
	gcc -o ../bin/paraNodeStatus $O ../lib/$(MACHTYPE)/paralib.a ../../lib/$(MACHTYPE)/jkweb.a
	strip ../bin/paraNodeStatus

