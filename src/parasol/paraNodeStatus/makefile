include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = paraNodeStatus.o

paraNodeStatus: $O $(MYLIBS)
	gcc -o $(HOME)/bin/$(MACHTYPE)/paraNodeStatus $O ../lib/$(MACHTYPE)/paralib.a ../../lib/$(MACHTYPE)/jkweb.a
	strip $(HOME)/bin/$(MACHTYPE)/paraNodeStatus

