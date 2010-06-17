include ../../inc/common.mk

L += -lm 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  ../lib/$(MACHTYPE)/paralib.a $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = broadNode.o

broadNode: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ../bin/broadNode $O $(MYLIBS) $L
	${STRIP} ../bin/broadNode${EXE}

clean::
	rm -f ${O}
