include ../../inc/common.mk

L += -lm 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  ../lib/$(MACHTYPE)/paralib.a $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = broadHub.o

broadHub: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ../bin/broadHub $O $(MYLIBS) $L
	${STRIP} ../bin/broadHub${EXE}

clean::
	rm -f ${O}
