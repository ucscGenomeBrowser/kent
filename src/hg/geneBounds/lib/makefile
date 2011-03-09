include ../../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = improbRunInfo.o

all: $O 
	echo all done in hg/geneBounds/lib/

clean::
	rm -f ${O}
